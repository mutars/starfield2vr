//
// Created by sergp on 6/23/2024.
//

#include "CreationEngineCameraManager.h"
#include "CreationEngineConstants.h"
#include "CreationEngineRendererModule.h"
#include "CreationEngineSingletonManager.h"
#include "RE/P/PlayerCamera.h"
#include "RE/S/ScaleformGFxMovie.h"
#include <CreationEngine/memory/offsets.h>
#include <CreationEngine/memory/ScanHelper.h>
#include <safetyhook/easy.hpp>
#include <CreationEngine/models/GameFlow.h>
#include <CreationEngine/models/ModSettingsStore.h>
#include <CreationEngine/ui/VRSettingsMenu.h>
#include <REL/Relocation.h>
#include <cstdlib>
#include <format>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <optional>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <mods/VR.hpp>

#include "ModSettings.h"
#include "StereoViewModule.h"
#include "body/BodyIK.h"
#include "vr/TrackingSpace.h"

namespace {
    float yaw_offset{0.0f};


}


void onSetNimFrustumDetour(RE::NiCamera *camera, RE::NiFrustum *frustum) {
    CreationEngineCameraManager::Get()->onSetNimFrustum(camera, frustum);
}

void onCalcNimFrustumDetour(RE::NiCamera *camera, float fov, float aspectRatio, float nearDist, float farDist,
                            char lodAdjust) {
    CreationEngineCameraManager::Get()->onCalcNiFrustum(camera, fov, aspectRatio, nearDist, farDist, lodAdjust);
}

void onScaleformSetViewPortDetour(uintptr_t *thisMovie, Scaleform::Gfx::Viewport *viewport) {
    CreationEngineCameraManager::Get()->onScaleformSetViewPort(thisMovie, viewport);
}

/*

void onScaleformMovieSetProjectionMatrix3DDetour(uintptr_t* thisMovie, Matrix4x4f& matrix)
{
    CreationEngineCameraManager::Get()->onScaleformMovieSetProjectionMatrix3D(thisMovie, matrix);
}
*/

bool isValidCamera(RE::NiCamera *pCamera) {
    return StereoViewModule::Get()->EyeOf(pCamera) >= 0;
}

void CreationEngineCameraManager::InstallHooks() {
    body::InstallModelNodeHook();
    REL::Relocation<uintptr_t> onNiAVObjectUpdateWorldAddr{GameStore::MemoryOffsets::BSFadeNode::vtable_UpdateWorld()};
    m_onNiAVObjectUpdateWorldHook = std::make_unique<FunctionHook>(onNiAVObjectUpdateWorldAddr.address(),
                                                                   reinterpret_cast<uintptr_t>(&onNiAVObjectUpdateWorld));
    m_onNiAVObjectUpdateWorldHook->create();


    REL::Relocation<uintptr_t> onGetCameraRotationAddr{ GameStore::MemoryOffsets::FirstPersonState::GetRotationQuatV() };
    m_onGetCameraRotationHook = std::make_unique<FunctionHook>(onGetCameraRotationAddr.address(), reinterpret_cast<uintptr_t>(&onFPSGetCameraRotation));
    m_onGetCameraRotationHook->create();


    REL::Relocation<uintptr_t> setNimFrustumVFuncAddr{GameStore::MemoryOffsets::NiCamera::SetFrustumVfunc()};
    m_onSetNimFrustumHook = std::make_unique<FunctionHook>(setNimFrustumVFuncAddr.address(),
                                                           reinterpret_cast<uintptr_t>(&onSetNimFrustumDetour));
    m_onSetNimFrustumHook->create();

    REL::Relocation<uintptr_t> calcNimFrustumVFuncAddr{GameStore::MemoryOffsets::NiCamera::CalcFrustumVfunc()};
    m_onCalcNimFrustumHook = std::make_unique<FunctionHook>(calcNimFrustumVFuncAddr.address(),
                                                            reinterpret_cast<uintptr_t>(&onCalcNimFrustumDetour));
    m_onCalcNimFrustumHook->create();

    REL::Relocation<uintptr_t> scaleformSetViewPortAddr{GameStore::MemoryOffsets::Scaleform::Movie::SetViewportVFunc()};
    m_onScaleformSetViewPortHook = std::make_unique<FunctionHook>(scaleformSetViewPortAddr.address(),
                                                                  reinterpret_cast<uintptr_t>(&onScaleformSetViewPortDetour));
    m_onScaleformSetViewPortHook->create();
}


void CreationEngineCameraManager::SnapshotAimPose() {
    tracking::Snapshot();
}

RE::NiAVObject *getCameraRootNode() {
    auto playerCamera = CreationEngineSingletonManager::GetPlayerCameraSingleton();

    if (!playerCamera || !playerCamera->IsInFirstPerson()) {
        return nullptr;
    }
    RE::NiAVObject *a_camera = playerCamera->pFirstPersonModeState->cameraRoot;
    while (a_camera) {
        if (a_camera->name == "Root") {
            return a_camera;
        }
        a_camera = a_camera->parent;
    }
    return nullptr;
}

void UpdateMesh(RE::NiAVObject* camera) {
    if (GameFlow::isImmovable() || GameFlow::isControlledByAI()) {
        return;
    }

    if(GameFlow::gStore.internalSettings.decoupledPitch && !(ModConstants::headTrackingType == 0 && GameFlow::isAimingDownSights())) {
        return;
    }
    static auto vr = VR::get();
    auto camera_quat = RE::NiQuaternion(camera->world.rotate);
    auto glm_camera_quat = glm::normalize(glm::quat(camera_quat.w, camera_quat.x, camera_quat.y, camera_quat.z));

    auto current_hmd_rotation = tracking::AimRotation();
    auto hmd_rotation_quat = glm::normalize(
            glm::quat_cast(current_hmd_rotation));
    hmd_rotation_quat = {hmd_rotation_quat.w, hmd_rotation_quat.x, -hmd_rotation_quat.z, hmd_rotation_quat.y};
    auto quat_out = glm_camera_quat;
    {
        if (GameFlow::gStore.internalSettings.pawnControl) {
            quat_out = glm::rotate(quat_out, -yaw_offset, glm::vec3{0.0f, 0.0f, 1.0f});
        }
        quat_out = quat_out * hmd_rotation_quat;
        camera_quat = RE::NiQuaternion(quat_out.w, quat_out.x, quat_out.y, quat_out.z);
        camera_quat.ToMatrix(camera->world.rotate);
    }
}

void CreationEngineCameraManager::onNiAVObjectUpdateWorld(RE::NiAVObject *obj, RE::NiUpdateData *a_data) {
    static auto instance = CreationEngineCameraManager::Get();
    using func_t = decltype(onNiAVObjectUpdateWorld);
    static auto original_func = instance->m_onNiAVObjectUpdateWorldHook->get_original<func_t>();
    static auto vr = VR::get();
    if (vr->is_hmd_active() && !ModSettings::showFlatScreenDisplay()) {
        // Third-person body: visibility at its root, then the head and right wrist as they update.
        body::OnUpdateWorld(obj, vr->m_engine_frame_count);

        auto camera_root = getCameraRootNode();
        if (obj->parent && camera_root && camera_root == obj->parent) {
            const bool is_mesh = !std::strstr(obj->name.c_str(), "Camera");
            original_func(obj, a_data);
            if (is_mesh) {
                UpdateMesh(obj);
            }
            return;
        }
    }
    original_func(obj, a_data);
}

void CreationEngineCameraManager::onScaleformSetViewPort(uintptr_t *thisMovie, Scaleform::Gfx::Viewport *viewport) {
    using func_t = decltype(onScaleformSetViewPortDetour);
    static auto original_func = m_onScaleformSetViewPortHook->get_original<func_t>();
    onScaleformSetViewPortInternal(thisMovie, viewport);
    original_func(thisMovie, viewport);
}

void
CreationEngineCameraManager::onScaleformSetViewPortInternal(uintptr_t *thisMovie, Scaleform::Gfx::Viewport *viewport) {

    static auto vr = VR::get();
    auto cc = reinterpret_cast<RE::Scaleform::GFx::MovieImpl *>(thisMovie);
    auto file_url = cc->GetMovieDef()->GetFileURL();
    GameFlow::renderMenu(file_url);
    VRSettingsMenu::OnMovieFrame(thisMovie, file_url);

    auto backbuffer_size = vr->get_backbuffer_size();
    auto viewport_buffer_width = viewport->bufferWidth;
    auto viewport_buffer_height = viewport->bufferHeight;

    auto settings = GameFlow::getMenuSettings(file_url);

    // Fullscreen menus keep the whole UI layer; they are shown on the flat screen.
    if (!vr->is_hmd_active() || ModSettings::showFlatScreenDisplay()) {
        return;
    }

    // One UI layer serves both eyes. The floating HUD panel gets all of it; its size in the world is set on the panel.
    const float scale = vr->is_native_hud_panel() ? 1.0f : settings.hud_scale;
    auto visible_width = std::min((int) ((float) backbuffer_size[0] * scale), viewport_buffer_width);
    auto visible_height = std::min((int) ((float) backbuffer_size[1] * scale), viewport_buffer_height);
    viewport->width = visible_width;
    viewport->height = visible_height;
    viewport->left = (int) (viewport_buffer_width - visible_width) / 2;
    viewport->top = (int) (viewport_buffer_height - visible_height) / 2;
}

void CreationEngineCameraManager::onSetNimFrustum(RE::NiCamera *pCamera, RE::NiFrustum *pFrustum) {
    using func_t = decltype(onSetNimFrustumDetour);
    static auto original_func = m_onSetNimFrustumHook->get_original<func_t>();
    onSetNiFrustumInternal(pCamera, pFrustum);
    original_func(pCamera, pFrustum);
}

void aiming_adjustments(Vector4f &frustum, float fov_adjustment) {
    frustum[0] = std::tanf(std::atanf(frustum[0]) - fov_adjustment);
    frustum[1] = std::tanf(std::atanf(frustum[1]) + fov_adjustment);
    frustum[2] = std::tanf(std::atanf(frustum[2]) + fov_adjustment);
    frustum[3] = std::tanf(std::atanf(frustum[3]) - fov_adjustment);
}

void CreationEngineCameraManager::onSetNiFrustumInternal(RE::NiCamera *pCamera, RE::NiFrustum *pFrustum) {
    if (!isValidCamera(pCamera)) {
        return;
    }
    static auto vr = VR::get();
    if (!vr->is_hmd_active()) {
        return;
    }
    auto eye = StereoViewModule::Get()->EyeOf(pCamera);
    auto runtime = vr->get_runtime();
    Vector4f frustum = runtime->frustums[eye];
    aiming_adjustments(frustum, get_fov_adjustment());
    pFrustum->left = frustum[0];
    pFrustum->right = frustum[1];
    pFrustum->top = frustum[2];
    pFrustum->bottom = frustum[3];
}

void CreationEngineCameraManager::onCalcNiFrustum(RE::NiCamera *pCamera, float fov, float aspectRatio, float nearz,
                                                  float farz, char lodAdjust) {
    using func_t = decltype(onCalcNimFrustumDetour);
    static auto original_func = m_onCalcNimFrustumHook->get_original<func_t>();
    auto playerCamera = CreationEngineSingletonManager::GetPlayerCameraSingleton();

    //    spdlog::info("Setting frustum for camera [{}] {} camera[{} {}] setFov[{}]", fmt::ptr(pCamera), pCamera->name, playerCamera->fov, playerCamera->notViewFov, fov);
    static auto vr = VR::get();

    if (vr->is_hmd_active() && CreationEngineSingletonManager::GetSceneGraphRoot()->worldCamera == pCamera) {
        //        fov = fov + Constants::lodAdjustFov;
        m_fov_adjust = fov - playerCamera->fov;
        // The first-person near plane (0.6) would cull the body and anything within reach before the eyes draw it.
        if (vr->is_native_stereo()) {
            nearz = std::min(nearz, 0.05f);
        }
        vr->m_nearz = nearz;
        vr->m_farz = farz;
    }
    original_func(pCamera, fov, aspectRatio, nearz, farz, lodAdjust);
}

float CreationEngineCameraManager::get_fov_adjustment() const {
    if (GameFlow::gStore.internalSettings.preventZoom) {
        return 0.0f;
    }
    return tanf((m_fov_adjust * ModConstants::DEG_TO_RAD) / 2.0f);
}

float CreationEngineCameraManager::get_head_tracking_multiplier() const {
    static auto playerCamera = CreationEngineSingletonManager::GetPlayerCameraSingleton();
    auto multiplier = (playerCamera->fov + m_fov_adjust) / playerCamera->fov;
    return multiplier * ModConstants::headTrackingMultiplier;
}


void CreationEngineCameraManager::UpdateWorldCamera() {
    static auto vr = VR::get();
    auto worldCamera = CreationEngineSingletonManager::GetSceneGraphRoot()->worldCamera;

    if (!worldCamera) {
        return;
    }

    static auto originalRotation = worldCamera->local.rotate;
    static auto originalPosition = worldCamera->local.translate;

    if (!vr->is_hmd_active() || ModConstants::cameraShake || ModSettings::showFlatScreenDisplay()) {
        worldCamera->local.rotate = originalRotation;
        worldCamera->local.translate = originalPosition;
        return;
    }

    // The world camera is the left eye and the right eye camera shares its parent.
    auto right_camera = vr->is_native_stereo() ? StereoViewModule::Get()->RightCamera() : nullptr;
    auto place = [&](RE::NiCamera* camera, const glm::mat4& eye_transform) {
        glm::mat4 local;
        if (!GameFlow::isImmovable() && !GameFlow::isControlledByAI() && GameFlow::isInFirstPerson()) {
            auto hmd_transform = vr->get_transform(0);
            hmd_transform[3] -= glm::vec4{ tracking::StandingOriginPosition(), 0.0f };
            const float tracking_scale = tracking::TrackingScale();
            hmd_transform[3]           = glm::vec4{ glm::vec3{ hmd_transform[3] } * tracking_scale, 1.0f };
            auto eye = eye_transform;
            eye[3]   = glm::vec4{ glm::vec3{ eye[3] } * tracking_scale, 1.0f };
            // The camera's parent carries the aim rotation; keep the view on the head.
            local = glm::inverse(tracking::CameraParentAimRotation(vr->m_engine_frame_count)) * hmd_transform * eye;
        } else {
            auto head_rotation = vr->get_transform(0);
            head_rotation[3] -= glm::vec4{ tracking::StandingOriginPosition(), 0.0f };
            local = head_rotation * eye_transform;
        }
        local = tracking::ToHavokSpace(local);
        camera->local.rotate = originalRotation * *(RE::NiMatrix3*) &local;
        camera->local.translate.x = local[3][0];
        camera->local.translate.y = local[3][1];
        camera->local.translate.z = local[3][2];
    };

    if (right_camera) {
        place(worldCamera, vr->get_eye_transform(VRRuntime::Eye::LEFT));
        place(right_camera, vr->get_eye_transform(VRRuntime::Eye::RIGHT));
        if (auto left_camera = StereoViewModule::Get()->LeftCamera()) {
            left_camera->local = worldCamera->local;
        }
    } else {
        // Before the eye views exist both eyes show the world camera's view.
        place(worldCamera, vr->get_eye_transform(VRRuntime::Eye::LEFT));
    }
}



void CreationEngineCameraManager::onFPSGetCameraRotation(RE::FirstPersonState *fps, RE::NiQuaternion *quat_out) {
    static auto instance = CreationEngineCameraManager::Get();
    static auto original_func = instance->m_onGetCameraRotationHook->get_original<decltype(onFPSGetCameraRotation)>();
    original_func(fps, quat_out);
    static auto vr = VR::get();
    if (!vr->is_hmd_active() || ModConstants::cameraShake || ModSettings::showFlatScreenDisplay()) {
        yaw_offset = 0.0f;
        return;
    }
    if (!GameFlow::isImmovable() && !GameFlow::isControlledByAI()) {
        // order of extraction Pitch->Yaw->Roll (Havok X->Z->Y)
        auto p_player = CreationEngineSingletonManager::GetPlayerRef();

        if (p_player) {
            if (const float snap = GameFlow::pendingSnapYaw.exchange(0.0f); snap != 0.0f) {
                const float two_pi = 2.0f * glm::pi<float>();
                p_player->data.angle.z = std::fmod(p_player->data.angle.z + snap + two_pi, two_pi);
                spdlog::info("[SnapTurn] Applied {:.0f} degrees on engine frame {}", glm::degrees(snap), vr->m_engine_frame_count);
            }
        }

        RE::NiMatrix3 havok_rotation;
        quat_out->ToMatrix(havok_rotation);
        float pitch, yaw, roll;
        havok_rotation.ToEulerAnglesXYZ(pitch, roll, yaw);

        auto current_hmd_rotation = vr->get_rotation(0);
        auto rotation_quat = glm::normalize(glm::quat_cast(tracking::ToHavokSpace(current_hmd_rotation)));
        auto ni_hmd_rotation = RE::NiQuaternion(rotation_quat.w, rotation_quat.x, rotation_quat.y, rotation_quat.z);
        const auto aim_rotation = tracking::AimRotation();
        const auto aim_quat = glm::normalize(glm::quat_cast(tracking::ToHavokSpace(aim_rotation)));
        const auto ni_aim_rotation = RE::NiQuaternion(aim_quat.w, aim_quat.x, aim_quat.y, aim_quat.z);
        {
            if (GameFlow::gStore.internalSettings.pawnControl) {
                yaw -= yaw_offset;
                havok_rotation.FromEulerAnglesXYZ(pitch, roll, yaw);

                auto new_quat = RE::NiQuaternion(havok_rotation);

                new_quat = new_quat * ni_hmd_rotation;

                auto delta = quat_out->InvertVector() * new_quat;
                delta.ToMatrix(havok_rotation);
                float delta_pitch, delta_yaw, delta_roll;
                havok_rotation.ToEulerAnglesXYZ(delta_pitch, delta_roll, delta_yaw);
                yaw_offset += delta_yaw;
                auto corrected_yaw = p_player->data.angle.z - delta_yaw + 2.0f * glm::pi<float>();
                corrected_yaw = std::fmod(corrected_yaw, 2.0f * glm::pi<float>());
                p_player->data.angle.z = corrected_yaw;
            }
            if (GameFlow::gStore.internalSettings.decoupledPitch && !((ModConstants::headTrackingType == 0 && GameFlow::isAimingDownSights()) || ModConstants::headTrackingType == 2)) {
                pitch = 0.0f;
            }
            havok_rotation.FromEulerAnglesXYZ(pitch, roll, yaw);
            const auto room_rotation = RE::NiQuaternion(havok_rotation);
            tracking::SetRoomRotation(room_rotation);
            *quat_out = room_rotation * ni_aim_rotation;
            tracking::RecordAppliedAim(aim_rotation, vr->m_engine_frame_count);
        }
    } else {
        yaw_offset = 0.0f;
        GameFlow::pendingSnapYaw.store(0.0f);
    }
}

RE::NiAVObject* CreationEngineCameraManager::GetBodyMuzzle()
{
    return body::GetBodyMuzzle();
}

bool CreationEngineCameraManager::GetBodyAimForward(float out[3])
{
    return body::GetBodyAimForward(out);
}

void CreationEngineCameraManager::NotifyPlayerFired()
{
    body::NotifyPlayerFired();
}
