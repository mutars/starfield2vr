//
// Created by sergp on 6/26/2024.
//

#include "PerfStats.h"
#include "CreationEngineRendererModule.h"
#include "CreationEngineCameraManager.h"
#include "CreationEngineConstants.h"
#include "CreationEngineSingletonManager.h"
#include "REL/Relocation.h"
#include <CreationEngine/memory/offsets.h>
#include <CreationEngine/models/GameFlow.h>
#include <CreationEngine/models/ModSettingsStore.h>
#include <_deps/directxtk12-src/Src/d3dx12.h>
#include <mods/VR.hpp>
#include <safetyhook/easy.hpp>

#include "ModSettings.h"
#include "StereoViewModule.h"

__int64 onRenderGraphRenderStartDetour(RE::CreationRendererPrivate::RenderGraph* rcx, RE::CreationRendererPrivate::RenderGraphData* pRenderGraphData, __int64 r8, __int64 r9)
{
    return CreationEngineRendererModule::Get()->onRenderGraphRenderStart(rcx, pRenderGraphData, r8, r9);
}

//__int64 onRenderFrameStartDetour(void* rcx, __int64 rdx, __int64 r8, __int64 r9)
//{
//    return CreationEngineRendererModule::Get()->onRenderFrameStart(rcx, rdx, r8, r9);
//}

std::unique_ptr<FunctionHook> m_onWindowMessageHook;

LRESULT CALLBACK WndProcDetour(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    using func_t              = decltype(WndProcDetour);
    static auto original_func = m_onWindowMessageHook->get_original<func_t>();
    //    spdlog::info("Window message hit {}", message);
    return original_func(hWnd, message, wParam, lParam);
}

void CreationEngineRendererModule::InstallHooks()
{
//    // 0x25dafe4 - this one works but sometimes give 2 ticks
//    REL::Relocation<uintptr_t> onWorldTickFn{ (uintptr_t )mod + 0x25e267c };
//    m_worldTick_hook = safetyhook::create_inline((void*)onWorldTickFn.address(), (void*)worldTick);
////        std::make_unique<FunctionHook>(onWorldTickFn.address(), reinterpret_cast<uint64_t>(&worldTick));
//    if(!m_worldTick_hook) {
//        spdlog::error("Failed to create hook for onWorldTick");
//    }

    REL::Relocation<uintptr_t> setReflexMarkerInternalFn{ GameStore::MemoryOffsets::Nvidia::onSetReflexMarkerInternal() };
    m_setReflexMarkerInternalHook = safetyhook::create_inline((void*)setReflexMarkerInternalFn.address(), (void*)setReflexMarkerInternal);
    if(!m_setReflexMarkerInternalHook) {
        spdlog::error("Failed to create hook for setReflexMarkerInternal");
    }

    //    REL::Relocation<RE::CreationEngineSettings**> settings{ REL::ID(878340) };
    REL::Relocation<RE::CreationEngineSettings**> settings{ GameStore::MemoryOffsets::GlobalRenderSettings() };
    m_creationEngineSettings = settings.get();

    //    REL::Relocation<uintptr_t> onRenderGraphRenderStartFuncAddr{ REL::ID(1079045) };
    REL::Relocation<uintptr_t> onRenderGraphRenderStartFuncAddr{ GameStore::MemoryOffsets::CreationRenderer::RenderGraphFrameStart() };
    m_onRenderGraphRenderStartHook = std::make_unique<FunctionHook>(onRenderGraphRenderStartFuncAddr.address(), reinterpret_cast<uint64_t>(&onRenderGraphRenderStartDetour));
    m_onRenderGraphRenderStartHook->create();

    // ID is exactly incrementing frames 149000
    // 202136 ID is function on start is frame start on end is frame end, however it is significantly decreases fps
    //    REL::Relocation<uintptr_t> onRenderFrameStartFuncAddr{ REL::ID(202136) };
//    REL::Relocation<uintptr_t> onRenderFrameStartFuncAddr{ GameStore::MemoryOffsets::CreationRenderer::RenderGraphRenderPipelineExecute() };
//    m_onRenderFrameStartHook = std::make_unique<FunctionHook>(onRenderFrameStartFuncAddr.address(), reinterpret_cast<uint64_t>(&onRenderFrameStartDetour));
//    m_onRenderFrameStartHook->create();
}

__int64 CreationEngineRendererModule::onRenderGraphRenderStart(RE::CreationRendererPrivate::RenderGraph* pGraph, RE::CreationRendererPrivate::RenderGraphData* pRenderGraphData,
                                                               __int64 i1, __int64 i2)
{
    using func_t              = decltype(onRenderGraphRenderStartDetour);
    static auto original_func = m_onRenderGraphRenderStartHook->get_original<func_t>();
    RenderGraphStart(pGraph, pRenderGraphData, true);
    auto result = original_func(pGraph, pRenderGraphData, i1, i2);
    RenderGraphStart(pGraph, pRenderGraphData, false);
    return result;
}

void CreationEngineRendererModule::RenderGraphStart(RE::CreationRendererPrivate::RenderGraph* pGraph, RE::CreationRendererPrivate::RenderGraphData* pRenderGraphData, bool before)
{
    if (m_startFramePass == nullptr && strcmp(pGraph->name, "CRBeginFrame") == 0) {
        m_startFramePass = pGraph;
    }
    if (m_startFramePass == pGraph && before) {
        GameFlow::resetGameState();
        ModSettings::g_internalSettings.showQuadDisplay = GameFlow::isShowingMenu();
    }
}

//__int64 CreationEngineRendererModule::onRenderFrameStart(void* pVoid, __int64 i, __int64 i1, __int64 i2)
//{
//    using func_t              = decltype(onRenderFrameStartDetour);
//    static auto original_func = m_onRenderFrameStartHook->get_original<func_t>();
////    spdlog::info("Frame Submit Start[{:X}], fc=[m={},r={}]", GetCurrentThreadId(), GameFlow::gameLoopFrameCount(), GameFlow::renderLoopFrameCount());
//    auto result = original_func(pVoid, i, i1, i2);
//    return result;
//}


void CreationEngineRendererModule::SetWindowSize(int width, int height)
{
    static std::atomic<bool> inside_change{ false };
    // Checked from the first frames on, so the intro screens already get the headset's size.
    constexpr int            kCheckInterval = 30;
    static int               last_synced_frame{ -kCheckInterval };
    auto                     fc = GameFlow::renderLoopFrameCount();
    if(inside_change.exchange(true)) {
        return;
    }

    if (m_creationEngineSettings == nullptr || *m_creationEngineSettings == nullptr || (fc - last_synced_frame) < kCheckInterval) {
        inside_change.store(false);
        return;
    }
    last_synced_frame = fc;
    inside_change.store(false);

    if (width == 0 || height == 0) {
        auto vr = VR::get();
        if (!vr->is_hmd_active()) {
            return;
        }
        width   = vr->get_hmd_width();
        height  = vr->get_hmd_height();
        if(width == 0 || height == 0) {
            return;
        }
    }

    // Compare the render target, not the window: Windows clamps bordered windows to the screen size.
    // The game puts its own size back after some loads and menus, so a mismatch is always corrected.
    constexpr int kMaxResizeAttempts = 1000000;
    static int    resize_attempts{ 0 };
    static int    last_target_width{ 0 };
    static int    last_target_height{ 0 };

    const auto backbuffer = VR::get()->get_backbuffer_size();
    if ((int)backbuffer[0] == width && (int)backbuffer[1] == height) {
        resize_attempts = 0;
        return;
    }
    if (width != last_target_width || height != last_target_height) {
        last_target_width  = width;
        last_target_height = height;
        resize_attempts    = 0;
    }
    if (resize_attempts >= kMaxResizeAttempts) {
        if (resize_attempts++ == kMaxResizeAttempts) {
            spdlog::warn("Render size is still {}x{} after {} resize requests for {}x{}; giving up", backbuffer[0], backbuffer[1], kMaxResizeAttempts, width, height);
        }
        return;
    }
    ++resize_attempts;

    spdlog::info("Setting window size to {} {} (back buffer {}x{}, attempt {})", width, height, backbuffer[0], backbuffer[1], resize_attempts);
    auto ce_rect = &(*m_creationEngineSettings)->displayGameSettings.displayRect;

    ce_rect->cx = width + ce_rect->x;
    ce_rect->cy = height + ce_rect->y;
    (*m_creationEngineSettings)->displayGameSettings.flags |= 0x100;
//    WINDOWPOS pos;
//    pos.hwnd            = (*m_creationEngineSettings)->pHwindow->windowHandle;
//    pos.hwndInsertAfter = nullptr;
//    pos.x               = rect->x;
//    pos.y               = rect->y;
//    pos.cx              = width;
//    pos.cy              = height;
//    pos.flags           = 0;
//    SendMessage((*m_creationEngineSettings)->pHwindow->windowHandle, WM_WINDOWPOSCHANGED, 0, reinterpret_cast<LPARAM>(&pos));
    auto  hWnd      = g_framework->get_window();
    RECT  rect      = { 0, 0, (LONG)width, (LONG)height };
    DWORD dwStyle   = GetWindowLong(hWnd, GWL_STYLE);
    DWORD dwExStyle = GetWindowLong(hWnd, GWL_EXSTYLE);
    BOOL  hasMenu   = GetMenu(hWnd) != NULL;
    AdjustWindowRectEx(&rect, dwStyle, hasMenu, dwExStyle);

    int nWidth  = rect.right - rect.left;
    int nHeight = rect.bottom - rect.top;
    spdlog::info("Setting window size to {} {}", nWidth, nHeight);
    SetWindowPos(hWnd, nullptr, 0, 0, nWidth, nHeight, SWP_ASYNCWINDOWPOS);
}

/*

void logWithExtraData(std::string_view message) {
    spdlog::info("[{:X}:{}] frameCount[m={},r={}]", GetCurrentThreadId(), message.data(), GameFlow::gameLoopFrameCount(), GameFlow::renderLoopFrameCount());
}
*/


uintptr_t CreationEngineRendererModule::setReflexMarkerInternal(uintptr_t rcx, uint32_t marker, uint32_t oldFrameIndex)
{
//    logWithExtraData(std::format("setReflexMarkerInternal[{}, f={}]", marker, oldFrameIndex));
    static auto instance = Get();
    static bool engine_notified = false;
    static     auto        vr            = VR::get();
    static auto cameraModule = CreationEngineCameraManager::Get();
    PerfStats::OnReflexMarker(marker, oldFrameIndex);
    if ((marker == 6 || marker == 0 || marker == 1) && !engine_notified) {
        engine_notified = true;
        instance->SetWindowSize(0,0);
        vr->on_wait_rendering(oldFrameIndex);
        vr->m_engine_frame_count = oldFrameIndex;
        vr->on_begin_rendering(oldFrameIndex);
        vr->update_hmd_state(oldFrameIndex);
        g_framework->run_imgui_frame(false);
        StereoViewModule::Get()->OnFrameStart();
        CreationEngineCameraManager::SnapshotAimPose();
        cameraModule->UpdateWorldCamera();
    }
    // Reset notification if marker is 1
    if (marker == 1) {
        engine_notified = false;
    }

    if (marker == 2) {
        vr->m_render_frame_count = oldFrameIndex;
    }

    if (marker == 4) {
        vr->m_presenter_frame_count = oldFrameIndex;
        PerfStats::OnPresent();
    }

    return instance->m_setReflexMarkerInternalHook.call<uintptr_t>(rcx, marker, oldFrameIndex);
}
