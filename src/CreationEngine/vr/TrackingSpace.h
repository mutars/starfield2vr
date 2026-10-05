#pragma once

#include <optional>

#include <glm/glm.hpp>
#include <RE/N/NiQuaternion.h>

// Tracked poses sampled once per engine frame and the mapping from OpenXR stage space into the game.
namespace tracking
{
    // Samples the aim rotation and the controller poses for this engine frame.
    void Snapshot();

    // Aim rotation (stage space) shared by the view, the aim and the meshes.
    glm::mat4 AimRotation();
    // The aim rotation currently baked into the game camera's parent transform.
    glm::mat4 AppliedAimRotation();
    void      RecordAppliedAim(const glm::mat4& rotation, int frame);
    // The rotation the camera's parent will carry this frame. If the game stopped updating its camera
    // (paused by a menu or popup), it still carries the last applied one.
    glm::mat4 CameraParentAimRotation(int frame);

    glm::mat4 RightHandPose();
    // Grip (palm) and aim poses of a controller, stage space.
    glm::mat4 GripPose(bool left);
    glm::mat4 AimPose(bool left);

    // Room orientation (game world, column-vector) maintained by the aim hook. It stays fixed while only the
    // head turns and changes with snap or game turning.
    void                     SetRoomRotation(const RE::NiQuaternion& q);
    std::optional<glm::mat3> RoomRotation();

    // Stage-space position recentering was done at.
    glm::vec3 StandingOriginPosition();

    // Scale applied to tracked head and hand movement so the player's eye height maps onto the character's.
    float TrackingScale();
    void  UpdateTrackingScale(float character_eye_height);

    // Stage-space transforms and vectors in the game's havok axes.
    glm::mat4 ToHavokSpace(const glm::mat4& mat);
    glm::vec3 ToHavokVector(const glm::vec3& v);
}
