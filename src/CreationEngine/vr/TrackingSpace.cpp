#include "TrackingSpace.h"

#include <algorithm>
#include <atomic>
#include <mutex>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <mods/VR.hpp>

#include <CreationEngine/CreationEngineConstants.h>
#include <CreationEngine/models/GameFlow.h>
#include <CreationEngine/models/ModSettingsStore.h>

namespace tracking
{
    namespace
    {
        const glm::mat4 permutation_pre = {
            1, 0, 0, 0,
            0, 0, 1, 0,
            0, -1, 0, 0,
            0, 0,  0, 1
        };
        const glm::mat4 permutation_post = glm::transpose(permutation_pre);

        std::mutex g_aim_mutex;
        glm::mat4  g_aim_rotation{ 1.0f };
        // The rotation last written into the game camera, and the engine frame it was written on.
        glm::mat4  g_applied_aim_rotation{ 1.0f };
        int        g_applied_aim_frame{ -1000 };
        // Right controller pose (stage space) sampled together with the aim rotation.
        glm::mat4  g_right_hand_pose{ 1.0f };

        // Grip poses (palm) of both controllers, sampled with the aim rotation.
        glm::mat4 g_grip_pose[2]{ glm::mat4{ 1.0f }, glm::mat4{ 1.0f } };
        glm::mat4 g_left_aim_pose{ 1.0f };

        std::mutex g_room_mutex;
        glm::mat3  g_room_rotation{ 1.0f };
        bool       g_has_room{ false };

        std::atomic<float> g_tracking_scale{ 1.0f };
    }

    void Snapshot()
    {
        static auto vr = VR::get();
        const bool hand = ModConstants::headTrackingType == ModConstants::kAimWithRightHand && vr->is_using_controllers();
        const auto rotation   = vr->get_rotation(hand ? vr->get_right_controller_index() : 0);
        const auto right_hand = vr->get_transform(vr->get_right_controller_index());
        const auto left_grip  = vr->get_grip_transform(vr->get_left_controller_index());
        const auto left_aim   = vr->get_transform(vr->get_left_controller_index());
        const auto right_grip = vr->get_grip_transform(vr->get_right_controller_index());
        std::scoped_lock _{ g_aim_mutex };
        g_grip_pose[0]    = left_grip;
        g_left_aim_pose   = left_aim;
        g_grip_pose[1]    = right_grip;
        g_aim_rotation    = rotation;
        g_right_hand_pose = right_hand;
    }

    glm::mat4 GripPose(bool left)
    {
        std::scoped_lock _{ g_aim_mutex };
        return g_grip_pose[left ? 0 : 1];
    }

    glm::mat4 RightHandPose()
    {
        std::scoped_lock _{ g_aim_mutex };
        return g_right_hand_pose;
    }

    glm::mat4 AimPose(bool left)
    {
        std::scoped_lock _{ g_aim_mutex };
        return left ? g_left_aim_pose : g_right_hand_pose;
    }

    void SetRoomRotation(const RE::NiQuaternion& q)
    {
        std::scoped_lock _{ g_room_mutex };
        g_room_rotation = glm::mat3_cast(glm::normalize(glm::quat{ q.w, q.x, q.y, q.z }));
        g_has_room      = true;
    }

    std::optional<glm::mat3> RoomRotation()
    {
        std::scoped_lock _{ g_room_mutex };
        return g_has_room ? std::optional<glm::mat3>{ g_room_rotation } : std::nullopt;
    }

    // The stored transform offset is the inverse of the recenter pose, so its translation column is not this point.
    glm::vec3 StandingOriginPosition()
    {
        static auto vr = VR::get();
        return glm::vec3{ glm::inverse(vr->get_transform_offset())[3] };
    }

    float TrackingScale() { return GameFlow::gStore.internalSettings.matchBodyHeight ? g_tracking_scale.load() : 1.0f; }

    // Re-measured after each recenter from the eye height above the floor (OpenXR stage space).
    void UpdateTrackingScale(float character_eye_height)
    {
        static glm::vec3 measured_origin{ -1000.0f };
        const auto       origin = StandingOriginPosition();
        if (glm::length(origin - measured_origin) < 1e-4f || character_eye_height < 0.5f || character_eye_height > 3.0f) {
            return;
        }
        static auto vr   = VR::get();
        const float real = vr->get_floor_eye_height();
        if (real < 0.0f) {
            return;  // floor height not known yet
        }
        measured_origin = origin;
        const float scale = real >= 1.3f && real <= 2.2f ? std::clamp(character_eye_height / real, 0.7f, 1.4f) : 1.0f;
        g_tracking_scale  = scale;
        spdlog::info("[Body] eye height: real {:.3f} m, character {:.3f} m -> tracking scale {:.3f}{}", real, character_eye_height, scale,
            real < 1.3f ? " (seated, not scaled)" : "");
    }

    glm::mat4 ToHavokSpace(const glm::mat4& mat)
    {
        return permutation_pre * mat * permutation_post;
    }

    glm::vec3 ToHavokVector(const glm::vec3& v)
    {
        return glm::vec3{ ToHavokSpace(glm::translate(glm::mat4{ 1.0f }, v))[3] };
    }

    glm::mat4 AppliedAimRotation()
    {
        std::scoped_lock _{ g_aim_mutex };
        return g_applied_aim_rotation;
    }

    glm::mat4 AimRotation()
    {
        std::scoped_lock _{ g_aim_mutex };
        return g_aim_rotation;
    }

    void RecordAppliedAim(const glm::mat4& rotation, int frame)
    {
        std::scoped_lock _{ g_aim_mutex };
        g_applied_aim_rotation = rotation;
        g_applied_aim_frame    = frame;
    }

    glm::mat4 CameraParentAimRotation(int frame)
    {
        std::scoped_lock _{ g_aim_mutex };
        return frame - g_applied_aim_frame <= 2 ? g_aim_rotation : g_applied_aim_rotation;
    }
}
