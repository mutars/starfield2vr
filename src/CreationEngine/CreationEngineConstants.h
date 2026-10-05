#pragma once

namespace ModConstants
{
    inline float headTrackingMultiplier = 1.0f;

    inline constexpr float DEG_TO_RAD = std::numbers::pi / 180;
    inline float lodAdjustFov = 0.0f;
    inline float fixedCameraRoll = 0.0f;
    inline bool preventCameraRoll{false};
    inline bool cameraShake{false};

    inline int headTrackingType{0};
    inline constexpr int kAimWithRightHand = 3;
}
