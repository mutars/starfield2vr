#include "MuzzleFire.h"

#include <safetyhook/easy.hpp>

#include <algorithm>
#include <cmath>

#include "CreationEngineConstants.h"
#include "CreationEngineCameraManager.h"
#include "CreationEngineSingletonManager.h"
#include <CreationEngine/memory/ScanHelper.h>
#include <mods/VR.hpp>

namespace MuzzleFire
{
    namespace
    {
        // ProjectileLaunchData offsets (1.16.244).
        constexpr std::size_t kShooterOffset       = 0x38;
        constexpr std::size_t kAimFromMuzzleOffset = 0xF2;
        constexpr std::size_t kMuzzleNodeOffset    = 0x88;  // NiPointer<NiAVObject>, checked before any lookup
        constexpr std::size_t kAnglesOffset        = 0x24;  // pitch, roll, yaw

        safetyhook::InlineHook g_compute_launch_origin_hook{};

        // Player shots take the projectile-node path when this flag is set; NPCs never read it.
        bool ComputeLaunchOrigin(std::uint8_t* launch_data)
        {
            static auto vr  = VR::get();
            bool        aim = false;
            if (launch_data && ModConstants::headTrackingType == ModConstants::kAimWithRightHand && vr->is_using_controllers()) {
                const auto player  = CreationEngineSingletonManager::GetPlayerRef();
                const auto shooter = *reinterpret_cast<void**>(launch_data + kShooterOffset);
                if (player && shooter == player) {
                    launch_data[kAimFromMuzzleOffset] = 1;
                    // Fire from the weapon in the shown body's hand; the launch data releases this reference.
                    auto& node = *reinterpret_cast<RE::NiAVObject**>(launch_data + kMuzzleNodeOffset);
                    if (!node) {
                        if (auto muzzle = CreationEngineCameraManager::GetBodyMuzzle()) {
                            InterlockedIncrement(reinterpret_cast<volatile long*>(reinterpret_cast<std::uint8_t*>(muzzle) + 8));
                            node = muzzle;
                            aim  = true;
                        }
                    }
                    CreationEngineCameraManager::NotifyPlayerFired();
                }
            }
            const bool result = g_compute_launch_origin_hook.call<bool>(launch_data);

            // Direction along the aim ray, whatever the muzzle node's own axis: pitch = -asin(z), yaw = atan2(x, y).
            float forward[3];
            if (aim && CreationEngineCameraManager::GetBodyAimForward(forward)) {
                const float two_pi = 6.28318530718f;
                auto        angles = reinterpret_cast<float*>(launch_data + kAnglesOffset);
                angles[0]          = -std::asin(std::clamp(forward[2], -1.0f, 1.0f));
                angles[1]          = 0.0f;
                angles[2]          = std::fmod(std::atan2(forward[0], forward[1]) + two_pi, two_pi);
            }
            return result;
        }
    }

    void Install()
    {
        const auto address = MemoryScan::FuncRelocation(
            "48 8B C4 55 53 56 57 41 54 41 55 41 56 41 57 48 8D A8 B8 FA FF FF 48 81 EC 08 06 00 00", 0x1b52510, 9101);
        if (!address) {
            spdlog::error("[MuzzleFire] ComputeLaunchOrigin not found");
            return;
        }
        g_compute_launch_origin_hook = safetyhook::create_inline(reinterpret_cast<void*>(address), reinterpret_cast<void*>(&ComputeLaunchOrigin));
        if (!g_compute_launch_origin_hook) {
            spdlog::error("[MuzzleFire] Failed to hook ComputeLaunchOrigin");
        }
    }
}
