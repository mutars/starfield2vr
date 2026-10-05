#include "StereoViewModule.h"

#include "CreationEngineRendererModule.h"
#include "CreationEngineSettings.h"
#include "CreationEngineSingletonManager.h"
#include "PerfStats.h"
#include "ModSettings.h"
#include <CreationEngine/memory/offsets.h>
#include <CreationEngine/memory/stereo_offsets.h>
#include <CreationEngine/models/GameFlow.h>
#include <CreationEngine/models/ModSettingsStore.h>
#include <Framework.hpp>
#include <RE/C/CreationRendererPrivate.h>
#include <RE/M/Main.h>
#include <RE/N/NiCamera.h>
#include <_deps/directxtk12-src/Src/d3dx12.h>
#include <chrono>
#include <format>
#include <intrin.h>
#include <mods/VR.hpp>
#include <nvidia/UpscalerAfrNvidiaModule.h>
#include <utility>

namespace
{
    namespace offsets = Steam::MemoryOffsets::Stereo;

    constexpr uint32_t kInvalidId = 0xFFFFFF;

    // StorageTable columns of the camera view table, as offsets into the table singleton.
    constexpr size_t kDirectionalShadowColumn = 0xF0;
    constexpr size_t kHighlightColumn         = 0x100;
    constexpr size_t kFeatureSetupColumn      = 0x108;
    constexpr size_t kImageSpaceColumn        = 0x110;
    constexpr size_t kCameraViewDataColumn    = 0x120;
    // Render graph table columns.
    constexpr size_t kMultiCameraViewColumn = 0x120;
    constexpr size_t kGraphOptionsColumn    = 0xE8;
    constexpr size_t kGraphFrameFlagColumn  = 0x108;
    constexpr size_t kGraphKeyColumn        = 0x128;

    constexpr size_t kCameraViewDataSize    = 0x1C;
    constexpr size_t kFeatureSetupSize      = 0x210;
    constexpr size_t kDirectionalShadowSize = 0x3C;
    constexpr size_t kHighlightSize         = 0x14;
    constexpr size_t kCameraViewIdOffset    = 0x4;  // CameraViewData::cameraHandleId
    constexpr size_t kCameraViewNearOffset  = 0x10; // CameraViewData::near
    // The game's first-person near plane (0.6) cuts off the player's body and anything within arm's reach in a headset.
    constexpr float  kEyeNearPlane           = 0.05f;
    constexpr size_t kAttachChildVtableIndex = 0x2A0 / 8;

    constexpr size_t kRootWorldCameraRoot = 0x78;
    constexpr size_t kRootMainView        = 0x3D0;
    constexpr size_t kRootMainRenderGraph = 0x3D8;
    constexpr size_t kRootMainGraphRecord = 0x3E0;

    constexpr size_t kCameraClipspaceType = 0x1D8;
    constexpr size_t kCameraMinNear       = 0x1DC;
    constexpr size_t kCameraAspect        = 0x1E0;
    constexpr size_t kCameraViewport      = 0x1E4;
    constexpr size_t kCameraScissors      = 0x1F4;
    constexpr size_t kPassCameraView      = 0x24;

    // A pass's command list for timing; null when the pass has none, instead of faulting.
    uintptr_t CommandListContextGuarded(void* render_graph_data)
    {
        __try {
            return static_cast<RE::CreationRendererPrivate::RenderGraphData*>(render_graph_data)->getCommandList();
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return 0;
        }
    }

    ID3D12GraphicsCommandList* TimingCommandList(void* render_graph_data)
    {
        if (render_graph_data == nullptr) {
            return nullptr;
        }
        auto context = reinterpret_cast<RE::RenderGraphDataD3D12Context*>(CommandListContextGuarded(render_graph_data));
        return context ? context->pID3D12CommandList : nullptr;
    }

    template <class T>
    T& At(void* base, size_t offset)
    {
        return *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + offset);
    }

    template <class F>
    F Fn(uintptr_t address)
    {
        return reinterpret_cast<F>(address);
    }

    uint8_t* StorageColumn(uintptr_t storage_global, size_t column)
    {
        auto storage = *reinterpret_cast<uint8_t**>(storage_global);
        return storage ? *reinterpret_cast<uint8_t**>(storage + column) : nullptr;
    }

    const uint8_t* StorageRow(uint8_t* column, uint32_t id, size_t row_size)
    {
        if (column == nullptr) {
            return nullptr;
        }
        auto indexes = *reinterpret_cast<uint32_t**>(column + 0x2C8);
        auto data    = *reinterpret_cast<uint8_t**>(column + 0x3C8);
        if (indexes == nullptr || data == nullptr) {
            return nullptr;
        }
        return data + (size_t)indexes[id & 0xFFFFFF] * row_size;
    }

    void CommitColumn(uint8_t* column)
    {
        const auto slot   = Fn<uint32_t (*)()>(offsets::StorageThreadSlot())();
        const auto writer = Fn<void* (*)(void*, uint32_t)>(offsets::StorageColumnWriter())(column + 0xC0, slot);
        Fn<void (*)(void*, void*)>(offsets::StorageColumnCommit())(column, writer);
    }

    void CameraIncRef(void* camera)
    {
        _InterlockedIncrement(&At<volatile long>(camera, 0x8));
    }

    void SetCameraRect(RE::NiCamera* camera, uintptr_t setter, const float rect[4])
    {
        float copy[4]{ rect[0], rect[1], rect[2], rect[3] };
        Fn<void (*)(RE::NiCamera*, float*)>(setter)(camera, copy);
    }

    bool SameRect(RE::NiCamera* camera, size_t offset, const float rect[4])
    {
        return std::memcmp(&At<float>(camera, offset), rect, sizeof(float) * 4) == 0;
    }

    struct RegisterContext
    {
        RE::Main::SceneGraphRoot* root;
        RE::NiCamera*             world_camera;
        RE::NiCamera*             camera;
        uint32_t                  view_id;
        uint32_t                  step;
        const char*               name;
    };

    // Mirrors the engine's own setup of the world camera and its view (scene root initialisation).
    void RegisterEngineObjects(RegisterContext* ctx)
    {
        ctx->step   = 1;
        auto camera = Fn<void* (*)(size_t)>(offsets::GameAllocate())(0x220);
        if (camera == nullptr) {
            return;
        }
        Fn<void* (*)(void*)>(offsets::NiCameraConstruct())(camera);
        CameraIncRef(camera);
        ctx->camera = static_cast<RE::NiCamera*>(camera);

        ctx->step         = 2;
        uintptr_t name    = 0;
        Fn<void (*)(uintptr_t*, const char*, uint8_t)>(offsets::FixedStringCreate())(&name, ctx->name, 0);
        std::swap(name, At<uintptr_t>(camera, 0x10));
        Fn<void (*)(uintptr_t*)>(offsets::FixedStringRelease())(&name);

        ctx->step = 3;
        Fn<void (*)(void*)>(offsets::NiCameraRegisterAsRenderCamera())(camera);
        At<float>(camera, kCameraMinNear) = At<float>(ctx->world_camera, kCameraMinNear);
        At<float>(camera, kCameraAspect)  = At<float>(ctx->world_camera, kCameraAspect);
        Fn<void (*)(void*, float, float)>(offsets::NiCameraSetNearFar())(camera, ctx->world_camera->viewFrustum._near, ctx->world_camera->viewFrustum._far);
        Fn<void (*)(void*, uint8_t)>(offsets::NiCameraSetClipspaceType())(camera, At<uint8_t>(ctx->world_camera, kCameraClipspaceType));
        ctx->camera->local = ctx->world_camera->local;

        ctx->step  = 4;
        auto owner = At<void*>(ctx->root, kRootWorldCameraRoot);
        auto vtbl  = *reinterpret_cast<uintptr_t**>(owner);
        Fn<void* (*)(void*, void*, bool)>(vtbl[kAttachChildVtableIndex])(owner, camera, true);

        ctx->step     = 5;
        ctx->view_id  = kInvalidId;
        Fn<void (*)(uint32_t*)>(offsets::CameraViewRegister())(&ctx->view_id);
        if (ctx->view_id == kInvalidId) {
            return;
        }

        // Exposure readback futures, laid out like the engine's own control blocks.
        ctx->step       = 6;
        const auto vtable = offsets::AverageLuminanceReadbackVtable();
        uintptr_t  readback_row[4]{};
        for (int i = 0; i < 2; ++i) {
            auto block = static_cast<uint8_t*>(Fn<void* (*)(size_t)>(offsets::SmallAllocate())(0x18));
            if (block == nullptr) {
                return;
            }
            std::memset(block, 0, 0x18);
            At<uintptr_t>(block, 0x0) = vtable;
            At<uint32_t>(block, 0x8)  = 1;
            At<uint32_t>(block, 0xC)  = 1;
            readback_row[i * 2]       = reinterpret_cast<uintptr_t>(block + 0x10);
            readback_row[i * 2 + 1]   = reinterpret_cast<uintptr_t>(block);
        }
        Fn<void (*)(uint32_t*, uintptr_t*)>(offsets::WriteAverageLuminanceReadback())(&ctx->view_id, readback_row);
        ctx->step = 7;
    }

    bool RegisterEngineObjectsGuarded(RegisterContext* ctx)
    {
        __try {
            RegisterEngineObjects(ctx);
            return ctx->step == 7;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    struct GraphContext
    {
        uint8_t*  record;
        uint8_t*  main_record;
        uint32_t* graph_id;
        uint8_t   main_key;
        uintptr_t main_name;
        uint32_t  step;
    };

    // Mirrors the scene root's setup of the main render graph (submission record, sort key, name, options).
    void RegisterGraphObjects(GraphContext* ctx)
    {
        ctx->step = 1;
        Fn<void (*)(uint32_t*)>(offsets::RenderGraphRegister())(ctx->graph_id);
        if (*ctx->graph_id == kInvalidId) {
            return;
        }

        // The engine builds a graph by its key and name, so the right eye's graph copies the main scene graph's
        // ("Frame"); the name string is shared, with its reference count raised for the copy.
        ctx->step = 2;
        struct
        {
            uint8_t   key;
            uint8_t   pad[7];
            uintptr_t name;
        } key{ ctx->main_key, {}, ctx->main_name };
        if (key.name) {
            _InterlockedIncrement(&At<volatile long>(reinterpret_cast<void*>(key.name), 0x10));
        }
        Fn<void (*)(uint32_t*, void*)>(offsets::WriteRenderGraphKey())(ctx->graph_id, &key);
        if (key.name) {
            Fn<void (*)(uintptr_t*)>(offsets::FixedStringRelease())(&key.name);
        }

        // An empty inline job list, then the main graph's jobs (context and feature flags) copied in.
        ctx->step = 3;
        std::memset(ctx->record, 0, 0x80);
        At<uint32_t>(ctx->record, 0x8)  = 0x80000000;
        At<uint32_t>(ctx->record, 0x78) = *ctx->graph_id;
        const auto count  = At<uint32_t>(ctx->main_record, 0x0);
        const auto local  = At<int32_t>(ctx->main_record, 0x8) < 0;
        auto       jobs   = local ? ctx->main_record + 0x10 : At<uint8_t*>(ctx->main_record, 0x10);
        for (uint32_t i = 0; jobs && i < count && i < 4; ++i) {
            auto job = jobs + 0x18 * (size_t)i;
            Fn<void (*)(void*, uint32_t, void*)>(offsets::AddRenderGraphJob())(ctx->record, At<uint32_t>(job, 0x10), At<void*>(job, 0x0));
        }
        if (count == 0) {
            return;
        }
        ctx->step = 4;
    }

    bool RegisterGraphObjectsGuarded(GraphContext* ctx)
    {
        __try {
            RegisterGraphObjects(ctx);
            return ctx->step == 4;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

} // namespace

struct StereoViewModule::ViewIdArray
{
    uint32_t size;
    uint32_t pad0;
    int32_t  capacity;  // sign bit set: the ids are stored inline
    uint32_t pad1;
    union
    {
        uint32_t  local[4];
        uint32_t* heap;
    };

    uint32_t* data() { return capacity < 0 ? local : heap; }
};
static_assert(sizeof(uintptr_t) == 8);

void StereoViewModule::InstallHooks()
{
    const std::pair<const char*, uintptr_t> required[]{
        { "NiCameraConstruct", offsets::NiCameraConstruct() },
        { "NiCameraRegisterAsRenderCamera", offsets::NiCameraRegisterAsRenderCamera() },
        { "NiCameraSetNearFar", offsets::NiCameraSetNearFar() },
        { "NiCameraSetViewport", offsets::NiCameraSetViewport() },
        { "NiCameraSetScissors", offsets::NiCameraSetScissors() },
        { "NiCameraSetClipspaceType", offsets::NiCameraSetClipspaceType() },
        { "CameraViewRegister", offsets::CameraViewRegister() },
        { "WriteCameraViewData", offsets::WriteCameraViewData() },
        { "WriteAverageLuminanceReadback", offsets::WriteAverageLuminanceReadback() },
        { "WriteImageSpaceData", offsets::WriteImageSpaceData() },
        { "WriteDirectionalShadowData", offsets::WriteDirectionalShadowData() },
        { "WriteHighlightSettings", offsets::WriteHighlightSettings() },
        { "BeginWriteFeatureSetup", offsets::BeginWriteFeatureSetup() },
        { "StorageThreadSlot", offsets::StorageThreadSlot() },
        { "StorageColumnWriter", offsets::StorageColumnWriter() },
        { "StorageColumnCommit", offsets::StorageColumnCommit() },
        { "SetMultiCameraViewData", offsets::SetMultiCameraViewData() },
        { "GameAllocate", offsets::GameAllocate() },
        { "SmallAllocate", offsets::SmallAllocate() },
        { "FixedStringCreate", offsets::FixedStringCreate() },
        { "FixedStringRelease", offsets::FixedStringRelease() },
        { "CameraViewStorage", offsets::CameraViewStorage() },
        { "RenderGraphStorage", offsets::RenderGraphStorage() },
        { "AverageLuminanceReadbackVtable", offsets::AverageLuminanceReadbackVtable() },
        { "RenderGraphRegister", offsets::RenderGraphRegister() },
        { "AddRenderGraphJob", offsets::AddRenderGraphJob() },
        { "SubmitRenderGraph", offsets::SubmitRenderGraph() },
        { "WriteRenderGraphOptions", offsets::WriteRenderGraphOptions() },
        { "WriteRenderGraphFrameFlag", offsets::WriteRenderGraphFrameFlag() },
        { "WriteRenderGraphKey", offsets::WriteRenderGraphKey() },
        { "NiCameraSetFrustum", GameStore::MemoryOffsets::NiCamera::SetFrustumVfunc() },
    };
    m_signatures_ok = true;
    for (auto& [name, address] : required) {
        if (address == 0) {
            spdlog::error("[Stereo] Signature not found: {}; both eyes will show the same image", name);
            m_signatures_ok = false;
        }
    }
    if (!m_signatures_ok) {
        return;
    }

    m_set_multi_view_hook = std::make_unique<FunctionHook>(offsets::SetMultiCameraViewData(), reinterpret_cast<uintptr_t>(&onSetMultiCameraViewData));
    m_set_multi_view_hook->create();
    m_submit_graph_hook = std::make_unique<FunctionHook>(offsets::SubmitRenderGraph(), reinterpret_cast<uintptr_t>(&onSubmitRenderGraph));
    m_submit_graph_hook->create();
    InstallUpscalerHooks();
    InstallLatePassHooks();
    if (auto vtable = reinterpret_cast<uintptr_t*>(
            MemoryScan::VTable("ScaleformCompositeRenderPass", ".?AVScaleformCompositeRenderPass@CreationRendererPrivate@@", 0))) {
        m_scaleform_composite_hook = std::make_unique<FunctionHook>(vtable[7], reinterpret_cast<uintptr_t>(&onScaleformComposite));
        m_scaleform_composite_hook->create();
    }
}

int StereoViewModule::EyeOf(const RE::NiCamera* camera) const
{
    if (camera == nullptr) {
        return -1;
    }
    if (camera == m_right_camera) {
        return 1;
    }
    if (camera == m_left_camera) {
        return 0;
    }
    auto root = CreationEngineSingletonManager::GetSceneGraphRoot();
    if (root && (camera == root->worldCamera || camera == root->starfieldScene.pStarFieldCamera || camera == root->starfieldScene.pGalaxyCamera)) {
        return 0;
    }
    return -1;
}

bool StereoViewModule::TryRegister()
{
    auto root = CreationEngineSingletonManager::GetSceneGraphRoot();
    if (root == nullptr || root->worldCamera == nullptr || root->worldCamera->cameraHandleID == kInvalidId || At<uint32_t>(root, kRootMainView) == kInvalidId ||
        At<void*>(root, kRootWorldCameraRoot) == nullptr) {
        return false;
    }

    RegisterContext right_ctx{ root, root->worldCamera, nullptr, kInvalidId, 0, "VR Right Eye Camera" };
    RegisterContext left_ctx{ root, root->worldCamera, nullptr, kInvalidId, 0, "VR Left Eye Camera" };
    if (!RegisterEngineObjectsGuarded(&right_ctx) || !RegisterEngineObjectsGuarded(&left_ctx)) {
        spdlog::error("[Stereo] Registering the eye views failed at step {}/{}; both eyes will show the same image", right_ctx.step, left_ctx.step);
        m_failed = true;
        return false;
    }
    m_left_camera    = left_ctx.camera;
    m_left_view_id   = left_ctx.view_id;
    m_right_camera   = right_ctx.camera;
    m_right_view_id  = right_ctx.view_id;
    m_left_mirrored  = {};
    m_right_mirrored = {};
    if (!RegisterRightGraph()) {
        spdlog::error("[Stereo] Registering the right eye's render graph failed; both eyes will show the same image");
        m_failed = true;
        return false;
    }
    m_registered = true;
    spdlog::info("[Stereo] Eye views registered: left {:x} (camera {:x}), right {:x} (camera {:x}); main view {:x}", m_left_view_id, m_left_camera->cameraHandleID,
                 m_right_view_id, m_right_camera->cameraHandleID, At<uint32_t>(root, kRootMainView));
    return true;
}

void StereoViewModule::OnFrameStart()
{
    static auto vr = VR::get();

    // Native presentation runs whenever the headset is active; until both eye views exist both eyes show the frame.
    const bool active = vr->is_hmd_active();
    vr->request_native_stereo(active);
    if (active && m_signatures_ok && !m_registered && !m_failed && vr->m_engine_frame_count - m_register_attempt_frame > 60) {
        m_register_attempt_frame = vr->m_engine_frame_count;
        TryRegister();
    }

    const bool stereo = vr->is_native_stereo() && m_registered;
    UpdateMenuFallback(stereo);
    KeepEyeViewportsFull();
    ApplyNativeShadowSettings(stereo);
    if (stereo) {
        DisableFrameGeneration();
    }
    const auto& settings = GameFlow::gStore.internalSettings;
    vr->set_native_hud_panel(stereo && settings.hudPanel, settings.hudPanelWidth, settings.hudPanelDistance);

    if (m_screenshot_requested.exchange(false)) {
        const auto stamp = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        vr->request_backbuffer_dump(Framework::get_persistent_dir(std::format("vr_eyes_{}.png", stamp)).wstring());
    }

    if (!stereo) {
        m_missed_appends = 0;
        vr->set_native_mono_frame(true);
        return;
    }

    MirrorMainView();
    UpdateEyeFrustums();

    // Without a fresh right eye graph this frame (a loading screen, a fullscreen menu) both eyes show the same image.
    m_missed_appends = m_appended.exchange(false) ? 0 : m_missed_appends + 1;
    vr->set_native_mono_frame(m_menu_fallback.load() || m_missed_appends > 1);
}

void StereoViewModule::KeepEyeViewportsFull()
{
    // Each eye renders the whole frame; the engine may reset a camera's rects, so they are reapplied when they differ.
    const float full[4]{ 0.0f, 1.0f, 1.0f, 0.0f };
    for (auto camera : { m_left_camera, m_right_camera }) {
        if (camera && (!SameRect(camera, kCameraViewport, full) || !SameRect(camera, kCameraScissors, full))) {
            SetCameraRect(camera, offsets::NiCameraSetViewport(), full);
            SetCameraRect(camera, offsets::NiCameraSetScissors(), full);
        }
    }
}

void StereoViewModule::MirrorMainView()
{
    auto root = CreationEngineSingletonManager::GetSceneGraphRoot();
    if (!m_registered || root == nullptr) {
        return;
    }
    const auto main_view = At<uint32_t>(root, kRootMainView);
    if (main_view == kInvalidId) {
        return;
    }
    MirrorInto(main_view, m_right_view_id, m_right_camera, m_right_mirrored);
    MirrorInto(main_view, m_left_view_id, m_left_camera, m_left_mirrored);
}

void StereoViewModule::MirrorInto(uint32_t main_view, uint32_t view_id, RE::NiCamera* camera, MirroredRows& mirrored)
{
    const auto storage = offsets::CameraViewStorage();

    MirroredRows current{};
    auto         read = [&](size_t column, size_t size, void* out) {
        auto row = StorageRow(StorageColumn(storage, column), main_view, size);
        if (row) {
            std::memcpy(out, row, size);
        }
        return row != nullptr;
    };
    if (!read(kCameraViewDataColumn, kCameraViewDataSize, current.camera_view_data) || !read(kImageSpaceColumn, sizeof(uint32_t), &current.image_space) ||
        !read(kFeatureSetupColumn, kFeatureSetupSize, current.feature_setup) || !read(kDirectionalShadowColumn, kDirectionalShadowSize, current.directional_shadow) ||
        !read(kHighlightColumn, kHighlightSize, current.highlight)) {
        return;
    }
    At<uint32_t>(current.camera_view_data, kCameraViewIdOffset) = camera->cameraHandleID;
    At<float>(current.camera_view_data, kCameraViewNearOffset)  = std::min(At<float>(current.camera_view_data, kCameraViewNearOffset), kEyeNearPlane);

    const bool first   = !mirrored.valid;
    auto       changed = [&](const void* a, const void* b, size_t size) { return first || std::memcmp(a, b, size) != 0; };
    auto       id      = view_id;

    if (changed(current.camera_view_data, mirrored.camera_view_data, kCameraViewDataSize)) {
        Fn<void (*)(uint32_t*, void*)>(offsets::WriteCameraViewData())(&id, current.camera_view_data);
    }
    if (changed(&current.image_space, &mirrored.image_space, sizeof(uint32_t))) {
        Fn<void (*)(uint32_t*, uint32_t*)>(offsets::WriteImageSpaceData())(&id, &current.image_space);
    }
    if (changed(current.directional_shadow, mirrored.directional_shadow, kDirectionalShadowSize)) {
        Fn<void (*)(uint32_t*, void*)>(offsets::WriteDirectionalShadowData())(&id, current.directional_shadow);
    }
    if (changed(current.highlight, mirrored.highlight, kHighlightSize)) {
        Fn<void (*)(uint32_t*, void*)>(offsets::WriteHighlightSettings())(&id, current.highlight);
    }
    if (changed(current.feature_setup, mirrored.feature_setup, kFeatureSetupSize)) {
        auto row = Fn<uint8_t* (*)(uint32_t)>(offsets::BeginWriteFeatureSetup())(id);
        if (row) {
            std::memcpy(row, current.feature_setup, kFeatureSetupSize);
            CommitColumn(StorageColumn(storage, kFeatureSetupColumn));
        }
    }
    if (first) {
        spdlog::info("[Stereo] Eye view {:x} mirrors main view {:x}", view_id, main_view);
    }
    current.valid = true;
    mirrored      = current;
}

void StereoViewModule::UpdateEyeFrustums()
{
    auto root = CreationEngineSingletonManager::GetSceneGraphRoot();
    if (!m_registered || root == nullptr || root->worldCamera == nullptr) {
        return;
    }
    auto world_camera = root->worldCamera;
    // The game moves its camera's near plane (much closer in first person than while loading); the eye cameras follow.
    const float near_plane = std::min(world_camera->viewFrustum._near, kEyeNearPlane);
    const float far_plane  = world_camera->viewFrustum._far;
    if (near_plane != m_eye_near || far_plane != m_eye_far) {
        m_eye_near = near_plane;
        m_eye_far  = far_plane;
        for (auto camera : { m_right_camera, m_left_camera }) {
            Fn<void (*)(void*, float, float)>(offsets::NiCameraSetNearFar())(camera, near_plane, far_plane);
        }
    }

    // Through the hooked setter, which fills in each camera's eye frustum.
    using set_frustum_t = void (*)(RE::NiCamera*, RE::NiFrustum*);
    static auto set_frustum = Fn<set_frustum_t>(GameStore::MemoryOffsets::NiCamera::SetFrustumVfunc());
    auto        world_frustum = world_camera->viewFrustum;
    set_frustum(world_camera, &world_frustum);
    for (auto camera : { m_left_camera, m_right_camera }) {
        At<float>(camera, kCameraMinNear) = At<float>(world_camera, kCameraMinNear);
        At<float>(camera, kCameraAspect)  = At<float>(world_camera, kCameraAspect);
        if (At<uint8_t>(camera, kCameraClipspaceType) != At<uint8_t>(world_camera, kCameraClipspaceType)) {
            Fn<void (*)(void*, uint8_t)>(offsets::NiCameraSetClipspaceType())(camera, At<uint8_t>(world_camera, kCameraClipspaceType));
        }
        auto frustum = world_camera->viewFrustum;
        set_frustum(camera, &frustum);
    }
}

uintptr_t StereoViewModule::onSetMultiCameraViewData(void* column, uint32_t graph_index, ViewIdArray* views)
{
    static auto instance = Get();
    using func_t         = decltype(onSetMultiCameraViewData);
    static auto original = instance->m_set_multi_view_hook->get_original<func_t>();
    static auto vr       = VR::get();

    // The main render graph draws the left eye's view instead of the world camera's.
    auto root = CreationEngineSingletonManager::GetSceneGraphRoot();
    if (instance->m_registered && vr->is_native_stereo() && !instance->m_menu_fallback.load() && views && root && views->size == 1 &&
        column == StorageColumn(offsets::RenderGraphStorage(), kMultiCameraViewColumn) && graph_index == (At<uint32_t>(root, kRootMainRenderGraph) & 0xFFFFFF) &&
        (views->data()[0] & 0xFFFFFF) == (At<uint32_t>(root, kRootMainView) & 0xFFFFFF)) {
        ViewIdArray left{};
        left.size     = 1;
        left.capacity = static_cast<int32_t>(0x80000004u);
        left.local[0] = instance->m_left_view_id;
        return original(column, graph_index, &left);
    }
    return original(column, graph_index, views);
}

namespace
{
    struct UpscalerPassInfo
    {
        const char* label;
        const char* rtti;
    };

    constexpr UpscalerPassInfo kUpscalerPasses[]{
        { "DLSS inputs", ".?AVDLSSProduceInputsRenderPass@CreationRendererPrivate@@" },
        { "DLSS", ".?AVDLSSUpscaleRenderPass@CreationRendererPrivate@@" },
    };

    template <int... Pass>
    constexpr std::array<uintptr_t, sizeof...(Pass)> UpscalerDetours(std::integer_sequence<int, Pass...>)
    {
        return { reinterpret_cast<uintptr_t>(&StereoViewModule::onUpscalerPass<Pass>)... };
    }
} // namespace

void StereoViewModule::InstallUpscalerHooks()
{
    static_assert(std::size(kUpscalerPasses) == kUpscalerPassCount);
    static const auto detours = UpscalerDetours(std::make_integer_sequence<int, kUpscalerPassCount>{});
    for (int i = 0; i < kUpscalerPassCount; ++i) {
        auto vtable = reinterpret_cast<uintptr_t*>(MemoryScan::VTable(kUpscalerPasses[i].label, kUpscalerPasses[i].rtti, 0));
        if (vtable == nullptr) {
            spdlog::warn("[Stereo] {} pass not found", kUpscalerPasses[i].label);
            continue;
        }
        m_upscaler_hooks[i] = std::make_unique<FunctionHook>(vtable[7], detours[i]);
        m_upscaler_hooks[i]->create();
    }
}

uintptr_t StereoViewModule::RunUpscalerPass(int pass_kind, void* pass, void* render_graph_data, void* pass_data)
{
    using func_t   = uintptr_t(void*, void*, void*);
    auto original  = m_upscaler_hooks[pass_kind]->get_original<func_t>();
    static auto vr = VR::get();
    if (!vr->is_native_stereo() || pass == nullptr) {
        return original(pass, render_graph_data, pass_data);
    }

    // DLSS keeps the right eye's history in a viewport of its own.
    const auto right_view = m_right_view_id & 0xFFFFFF;
    const bool right      = (At<uint32_t>(pass, kPassCameraView) & 0xFFFFFF) == right_view || (SceneOf(render_graph_data) & 0xFFFFFF) == right_view;
    auto list = TimingCommandList(render_graph_data);
    if (pass_kind == kDLSSInputs) {
        PerfStats::MarkGpu(list, PerfStats::GpuPoint::kEyeUpscaleStart, right ? 1 : 0);
    }
    UpscalerAfrNvidiaModule::set_secondary_view(right);
    auto result = original(pass, render_graph_data, pass_data);
    UpscalerAfrNvidiaModule::set_secondary_view(false);
    if (pass_kind == kDLSSUpscale) {
        PerfStats::MarkGpu(list, PerfStats::GpuPoint::kEyeUpscaleEnd, right ? 1 : 0);
    }
    return result;
}

uint32_t StereoViewModule::SceneOf(void* render_graph_data) const
{
    if (render_graph_data == nullptr) {
        return kInvalidId;
    }
    const auto viewport = At<uint32_t>(render_graph_data, 0x140);
    if (viewport >= 8) {
        return kInvalidId;
    }
    auto base = At<uint8_t*>(render_graph_data, 0x108 + 8 * (size_t)viewport);
    return base ? At<uint32_t>(base, 0x24) : kInvalidId;
}

uintptr_t StereoViewModule::onScaleformComposite(void* pass, void* render_graph_data, void* pass_data)
{
    static auto instance = Get();
    using func_t         = uintptr_t(void*, void*, void*);
    static auto original = instance->m_scaleform_composite_hook->get_original<func_t>();
    static auto vr       = VR::get();
    if (vr->is_native_stereo() && pass_data != nullptr) {
        instance->CaptureUiLayer(render_graph_data, pass_data);
    }
    return original(pass, render_graph_data, pass_data);
}

bool StereoViewModule::RegisterRightGraph()
{
    auto root = CreationEngineSingletonManager::GetSceneGraphRoot();
    if (root == nullptr || At<uint32_t>(root, kRootMainRenderGraph) == kInvalidId) {
        return false;
    }
    auto main_record = reinterpret_cast<uint8_t*>(root) + kRootMainGraphRecord;
    auto key_row     = StorageRow(StorageColumn(offsets::RenderGraphStorage(), kGraphKeyColumn), At<uint32_t>(root, kRootMainRenderGraph), 0x10);
    if (key_row == nullptr) {
        return false;
    }
    GraphContext ctx{ m_right_graph_record, main_record, &m_right_graph_id, key_row[0], *reinterpret_cast<const uintptr_t*>(key_row + 8), 0 };
    spdlog::info("[Stereo] Main render graph key {:x}, name {:x}", ctx.main_key, ctx.main_name);
    if (!RegisterGraphObjectsGuarded(&ctx)) {
        spdlog::error("[Stereo] Right eye render graph setup stopped at step {}", ctx.step);
        return false;
    }
    std::memset(m_right_graph_options, 0, sizeof(m_right_graph_options));
    m_right_graph_frame_flag = 0xFF;
    spdlog::info("[Stereo] Right eye render graph {:x} registered with {} job(s); main graph {:x}", m_right_graph_id, At<uint32_t>(main_record, 0x0),
                 At<uint32_t>(root, kRootMainRenderGraph));
    return true;
}

void StereoViewModule::PrepareRightGraph()
{
    auto root = CreationEngineSingletonManager::GetSceneGraphRoot();
    if (root == nullptr) {
        return;
    }
    const auto main_graph = At<uint32_t>(root, kRootMainRenderGraph);
    const auto storage    = offsets::RenderGraphStorage();
    auto       id         = m_right_graph_id;

    // The main graph's options and per-frame flag, copied when they change.
    if (auto row = StorageRow(StorageColumn(storage, kGraphOptionsColumn), main_graph, sizeof(m_right_graph_options))) {
        if (std::memcmp(row, m_right_graph_options, sizeof(m_right_graph_options)) != 0) {
            std::memcpy(m_right_graph_options, row, sizeof(m_right_graph_options));
            uint8_t options[sizeof(m_right_graph_options)];
            std::memcpy(options, row, sizeof(options));
            Fn<void (*)(uint32_t*, void*)>(offsets::WriteRenderGraphOptions())(&id, options);
        }
    }
    if (auto row = StorageRow(StorageColumn(storage, kGraphFrameFlagColumn), main_graph, 1)) {
        if (*row != m_right_graph_frame_flag) {
            m_right_graph_frame_flag = *row;
            uint8_t flag             = *row;
            Fn<void (*)(uint32_t*, uint8_t*)>(offsets::WriteRenderGraphFrameFlag())(&id, &flag);
        }
    }

    // The right graph renders only the right eye's view.
    ViewIdArray views{};
    views.size     = 1;
    views.capacity = static_cast<int32_t>(0x80000004u);
    views.local[0] = m_right_view_id;
    m_set_multi_view_hook->get_original<decltype(onSetMultiCameraViewData)>()(StorageColumn(storage, kMultiCameraViewColumn), id & 0xFFFFFF, &views);
    m_appended.store(true);
}

uintptr_t StereoViewModule::onSubmitRenderGraph(void* frame_list, void* record)
{
    static auto instance = Get();
    using func_t         = uintptr_t(void*, void*);
    static auto original = instance->m_submit_graph_hook->get_original<func_t>();
    static auto vr       = VR::get();

    // The right eye's graph goes in just ahead of the main graph, which renders the left eye.
    auto root = CreationEngineSingletonManager::GetSceneGraphRoot();
    if (instance->m_registered && vr->is_native_stereo() && !instance->m_menu_fallback.load() && root &&
        record == reinterpret_cast<uint8_t*>(root) + kRootMainGraphRecord && At<uint32_t>(root, kRootMainView) != kInvalidId) {
        instance->PrepareRightGraph();
        original(frame_list, instance->m_right_graph_record);
    }
    return original(frame_list, record);
}

void StereoViewModule::UpdateMenuFallback(bool stereo)
{
    const bool fallback = stereo && ModSettings::showFlatScreenDisplay();
    if (fallback != m_menu_fallback.load()) {
        m_menu_fallback.store(fallback);
        spdlog::info("[Stereo] {}", fallback ? "Fullscreen menu: the game's own view is shown on the flat screen" : "Fullscreen menu closed: both eye views render again");
    }
}

void StereoViewModule::ApplyNativeShadowSettings(bool stereo)
{
    // Every eye view runs the dynamic shadow selection, but the per-light fade state is shared: each eye resets the fades of
    // lights only the other eye sees, so those shadows keep restarting their tiled fade. The main view whose LOD the
    // dynamic shadow maps borrow is not rendered in native stereo, so each shadow map picks its own.
    if (stereo == m_shadow_settings_applied) {
        return;
    }
    m_shadow_settings_applied = stereo;
    auto settings = CreationEngineSettings::Get();
    using Type    = CreationEngineSettings::SettingType;
    constexpr auto kFade = "fDynamicShadowFadeSeconds:Shadows";
    constexpr auto kLod  = "bDynamicShadowmapsUseMainViewLOD:Shadows";
    if (stereo) {
        m_saved_shadow_fade_seconds  = settings->get_setting(kFade, Type::kINISetting, 0.75f);
        m_saved_shadow_main_view_lod = settings->get_setting(kLod, Type::kINISetting, true);
    }
    const bool fade = settings->set_setting(kFade, Type::kINISetting, stereo ? 0.0f : m_saved_shadow_fade_seconds);
    const bool lod  = settings->set_setting(kLod, Type::kINISetting, stereo ? false : m_saved_shadow_main_view_lod);
    spdlog::info("[Stereo] Shadow settings {}: dynamic shadow fade {}s ({}), main view LOD {} ({})", stereo ? "for native stereo" : "restored",
                 settings->get_setting(kFade, Type::kINISetting, -1.0f), fade ? "set" : "not found", settings->get_setting(kLod, Type::kINISetting, false),
                 lod ? "set" : "not found");
}

void StereoViewModule::CaptureEyeImage(uint32_t eye, int pass_kind, void* render_graph_data, void* pass_data)
{
    static auto vr    = VR::get();
    auto        data  = static_cast<RE::CreationRendererPrivate::RenderPassData*>(pass_data);
    auto        graph = static_cast<RE::CreationRendererPrivate::RenderGraphData*>(render_graph_data);
    if (data->renderPassItems == nullptr) {
        return;
    }
    auto context = reinterpret_cast<RE::RenderGraphDataD3D12Context*>(graph->getCommandList());
    if (context == nullptr || context->pID3D12CommandList == nullptr) {
        return;
    }
    auto command_list = context->pID3D12CommandList;

    // The tonemap pass writes the eye image (its largest written texture); each later pass is captured again when it
    // writes a texture of that size and format, so the last post effect of the chain is what the eye shows.
    const uint32_t count = (uint32_t)data->renderPassItems->_size;
    ID3D12Resource*       output{ nullptr };
    D3D12_RESOURCE_STATES output_state{};
    for (uint32_t i = 0; i < count && i < 16; ++i) {
        auto item     = data->getRenderPassItemByIndex(i);
        auto resource = data->getNativeResourceByIndex(i);
        if (item == nullptr || resource == nullptr) {
            continue;
        }
        const auto desc  = resource->GetDesc();
        const auto state = (D3D12_RESOURCE_STATES)RE::CreationRendererPrivate::RenderPassItem::getDXGIState(item->stateOrFlags);
        const bool written = (state & (D3D12_RESOURCE_STATE_RENDER_TARGET | D3D12_RESOURCE_STATE_UNORDERED_ACCESS)) != 0;
        if (!written || desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || desc.SampleDesc.Count != 1 || desc.Width < 256) {
            continue;
        }
        const auto& expected = m_eye_output_desc[eye];
        // Sharpening writes into an output-sized texture a few pixels larger than the tonemap output.
        if (pass_kind != 0 && (desc.Width + 16 < expected.Width || desc.Height + 16 < expected.Height || desc.Width > expected.Width + 64 ||
                               desc.Height > expected.Height + 64 || desc.Format != expected.Format)) {
            continue;
        }
        if (output == nullptr || desc.Width * desc.Height > output->GetDesc().Width * output->GetDesc().Height) {
            output       = resource;
            output_state = state;
        }
    }
    if (output == nullptr) {
        return;
    }

    const auto desc    = output->GetDesc();
    if (pass_kind == 0) {
        m_eye_output_desc[eye] = desc;
    }
    Microsoft::WRL::ComPtr<ID3D12Resource> capture{};
    for (auto& candidate : m_eye_capture[eye]) {
        const auto candidate_desc = candidate->GetDesc();
        if (candidate_desc.Width == desc.Width && candidate_desc.Height == desc.Height && candidate_desc.Format == desc.Format) {
            capture = candidate;
            break;
        }
    }
    if (capture == nullptr) {
        if (m_eye_capture[eye].size() >= 4) {
            m_eye_capture[eye].erase(m_eye_capture[eye].begin());
        }
        auto capture_desc  = CD3DX12_RESOURCE_DESC::Tex2D(desc.Format, desc.Width, desc.Height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
        const CD3DX12_HEAP_PROPERTIES heap{ D3D12_HEAP_TYPE_DEFAULT };
        auto device = g_framework->get_d3d12_hook()->get_device();
        if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &capture_desc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&capture)))) {
            spdlog::error("[Stereo] Failed to create the {} eye capture texture", eye == 0 ? "left" : "right");
            return;
        }
        capture->SetName(eye == 0 ? L"Native stereo left eye capture" : L"Native stereo right eye capture");
        spdlog::info("[Stereo] {} eye image {}x{} format {}", eye == 0 ? "Left" : "Right", desc.Width, desc.Height, (uint32_t)desc.Format);
        m_eye_capture[eye].push_back(capture);
    }

    D3D12_RESOURCE_BARRIER to_copy[]{
        CD3DX12_RESOURCE_BARRIER::Transition(output, output_state, D3D12_RESOURCE_STATE_COPY_SOURCE),
        CD3DX12_RESOURCE_BARRIER::Transition(capture.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST),
    };
    command_list->ResourceBarrier(2, to_copy);
    CD3DX12_TEXTURE_COPY_LOCATION dst{ capture.Get(), 0 };
    CD3DX12_TEXTURE_COPY_LOCATION src{ output, 0 };
    command_list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    D3D12_RESOURCE_BARRIER restore[]{
        CD3DX12_RESOURCE_BARRIER::Transition(output, D3D12_RESOURCE_STATE_COPY_SOURCE, output_state),
        CD3DX12_RESOURCE_BARRIER::Transition(capture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
    };
    command_list->ResourceBarrier(2, restore);
    vr->set_native_eye_source(eye, capture.Get());
}

void StereoViewModule::DisableFrameGeneration()
{
    // Generated frames are interpolated from consecutive presented images, which in native stereo hold the eyes in turn.
    if (!m_frame_generation_looked_up) {
        m_frame_generation_looked_up = true;
        for (auto type : { CreationEngineSettings::SettingType::kINIPrefSetting, CreationEngineSettings::SettingType::kINISetting }) {
            if ((m_frame_generation_setting = CreationEngineSettings::get_setting("uiFrameGenerationTech:Display", type)) != nullptr) {
                break;
            }
        }
        if (m_frame_generation_setting == nullptr) {
            spdlog::warn("[Stereo] Frame generation setting not found");
        }
    }
    auto setting = static_cast<RE::Setting*>(m_frame_generation_setting);
    if (setting && setting->GetValue<uint32_t>(0) != 0) {
        spdlog::info("[Stereo] Frame generation {} turned off for native stereo", setting->GetValue<uint32_t>(0));
        setting->SetValue<uint32_t>(0);
    }
}

void StereoViewModule::CaptureUiLayer(void* render_graph_data, void* pass_data)
{
    static auto vr    = VR::get();
    auto        data  = static_cast<RE::CreationRendererPrivate::RenderPassData*>(pass_data);
    auto        graph = static_cast<RE::CreationRendererPrivate::RenderGraphData*>(render_graph_data);
    if (data->renderPassItems == nullptr || data->renderPassItems->_size == 0) {
        return;
    }
    auto context = reinterpret_cast<RE::RenderGraphDataD3D12Context*>(graph->getCommandList());
    auto item    = data->getRenderPassItemByIndex(0);
    auto layer   = data->getNativeResourceByIndex(0);
    if (context == nullptr || context->pID3D12CommandList == nullptr || item == nullptr || layer == nullptr) {
        return;
    }
    // The first texture of the composite is the UI layer.
    const auto desc  = layer->GetDesc();
    const auto state = (D3D12_RESOURCE_STATES)RE::CreationRendererPrivate::RenderPassItem::getDXGIState(item->stateOrFlags);
    if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || desc.SampleDesc.Count != 1 || desc.Width < 64) {
        return;
    }
    if (m_ui_capture == nullptr || m_ui_capture->GetDesc().Width != desc.Width || m_ui_capture->GetDesc().Height != desc.Height || m_ui_capture->GetDesc().Format != desc.Format) {
        m_ui_capture.Reset();
        auto capture_desc = CD3DX12_RESOURCE_DESC::Tex2D(desc.Format, desc.Width, desc.Height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
        const CD3DX12_HEAP_PROPERTIES heap{ D3D12_HEAP_TYPE_DEFAULT };
        if (FAILED(g_framework->get_d3d12_hook()->get_device()->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &capture_desc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                                                                       nullptr, IID_PPV_ARGS(&m_ui_capture)))) {
            spdlog::error("[Stereo] Failed to create the UI layer capture texture");
            return;
        }
        m_ui_capture->SetName(L"Native stereo UI layer capture");
        spdlog::info("[Stereo] UI layer captured for both eyes: {}x{} format {} state {:x}", desc.Width, desc.Height, (uint32_t)desc.Format, (uint32_t)state);
    }
    auto command_list = context->pID3D12CommandList;
    D3D12_RESOURCE_BARRIER to_copy[]{
        CD3DX12_RESOURCE_BARRIER::Transition(layer, state, D3D12_RESOURCE_STATE_COPY_SOURCE),
        CD3DX12_RESOURCE_BARRIER::Transition(m_ui_capture.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST),
    };
    command_list->ResourceBarrier(2, to_copy);
    CD3DX12_TEXTURE_COPY_LOCATION dst{ m_ui_capture.Get(), 0 };
    CD3DX12_TEXTURE_COPY_LOCATION src{ layer, 0 };
    command_list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    D3D12_RESOURCE_BARRIER restore[]{
        CD3DX12_RESOURCE_BARRIER::Transition(layer, D3D12_RESOURCE_STATE_COPY_SOURCE, state),
        CD3DX12_RESOURCE_BARRIER::Transition(m_ui_capture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
    };
    command_list->ResourceBarrier(2, restore);
    vr->set_native_ui_source(m_ui_capture.Get());
}

namespace
{
    struct LatePassInfo
    {
        const char* label;
        const char* rtti;
    };

    // The tonemap pass first; the rest are post effects that may follow it.
    constexpr LatePassInfo kLatePasses[]{
        { "HDRCompositeRenderPass", ".?AVHDRCompositeRenderPass@CreationRendererPrivate@@" },
        { "ContrastAdaptiveSharpeningRenderPass", ".?AVContrastAdaptiveSharpeningRenderPass@CreationRendererPrivate@@" },
        { "PostSharpenRenderPass", ".?AVPostSharpenRenderPass@CreationRendererPrivate@@" },
        { "FilmGrainRenderPass", ".?AVFilmGrainRenderPass@CreationRendererPrivate@@" },
        { "VignetteRenderPass", ".?AVVignetteRenderPass@CreationRendererPrivate@@" },
        { "LensFlare_AlphaBlendRenderPass", ".?AVLensFlare_AlphaBlendRenderPass@CreationRendererPrivate@@" },
        { "LensFlareDrawRenderPass", ".?AVLensFlareDrawRenderPass@CreationRendererPrivate@@" },
    };

    template <int... Pass>
    constexpr std::array<uintptr_t, sizeof...(Pass)> LateDetours(std::integer_sequence<int, Pass...>)
    {
        return { reinterpret_cast<uintptr_t>(&StereoViewModule::onLatePass<Pass>)... };
    }
} // namespace

void StereoViewModule::InstallLatePassHooks()
{
    static_assert(std::size(kLatePasses) == kLatePassCount);
    static const auto detours = LateDetours(std::make_integer_sequence<int, kLatePassCount>{});
    for (int i = 0; i < kLatePassCount; ++i) {
        auto vtable = reinterpret_cast<uintptr_t*>(MemoryScan::VTable(kLatePasses[i].label, kLatePasses[i].rtti, 0));
        if (vtable == nullptr) {
            spdlog::warn("[Stereo] {} not found", kLatePasses[i].label);
            continue;
        }
        m_late_hooks[i] = std::make_unique<FunctionHook>(vtable[7], detours[i]);
        m_late_hooks[i]->create();
    }
}

uintptr_t StereoViewModule::RunLatePass(int pass_kind, void* pass, void* render_graph_data, void* pass_data)
{
    using func_t     = uintptr_t(void*, void*, void*);
    auto        original = m_late_hooks[pass_kind]->get_original<func_t>();
    static auto vr       = VR::get();

    const auto result = original(pass, render_graph_data, pass_data);
    if (!vr->is_native_stereo() || pass_data == nullptr) {
        return result;
    }
    const int eye = EyeOfGraph(render_graph_data);
    if (eye >= 0) {
        CaptureEyeImage((uint32_t)eye, pass_kind, render_graph_data, pass_data);
        // The last post effect of the eye's chain overwrites this point, so it ends up marking the chain's end.
        PerfStats::MarkGpu(TimingCommandList(render_graph_data), PerfStats::GpuPoint::kEyePostEnd, eye);
    }
    return result;
}

int StereoViewModule::EyeOfGraph(void* render_graph_data) const
{
    const auto scene = SceneOf(render_graph_data) & 0xFFFFFF;
    if (scene == kInvalidId) {
        return -1;
    }
    if (scene == (m_left_view_id & 0xFFFFFF)) {
        return 0;
    }
    if (scene == (m_right_view_id & 0xFFFFFF)) {
        return 1;
    }
    return -1;
}

