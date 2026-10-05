#pragma once
#include "glm/glm.hpp"
#include <CreationEngine/memory/ScanHelper.h>
#include <CreationEngine/memory/offsets.h>
#include <glm/detail/type_quat.hpp>
#include <glm/gtx/matrix_major_storage.hpp>

namespace GameFlow
{
    struct DebugData {
        std::vector<glm::vec3> points{};
        std::vector<std::string_view> ui_parts{};
    };

    struct Settings
    {
        struct HudSettings {
            float hudScale{0.4f};
            int perspective{150};
        } hudSettings{};
        struct InternalSettings {
            int headAimingAbsolute{1};
            float flatScreenDistance{1.5f};
            bool preventZoom{false};
            bool alternativeJoyLayout{false};
            int  controllerLayout{0}; // 0 = matching letters (Quest/Touch), 1 = legacy
            bool recenterAfterLoading{true};
            bool matchBodyHeight{true};
            bool bodyLean{true};
            bool bodyFacing{true};
            bool supportHand{true};
            bool fingerPoses{true};
            bool walkingLegs{true};
            bool bodyTracking{true};
            bool handTracking{true};
            bool hudPanel{true};
            float hudPanelWidth{1.86f};
            float hudPanelDistance{2.0f};
            int   turnMode{0}; // 0 = snap, 1 = smooth (game's own stick look)
            float snapTurnDegrees{45.0f};
            bool  turnFade{false};
            float smoothTurnSpeed{0.7f};
            bool  stickPitch{false};
            int   moveDirection{1}; // 0 = head, 1 = left hand
            float speedLimit{1.0f};
            bool  smoothAcceleration{true};
            float vignetteStrength{0.6f};
            bool  weaponFollowsHand{false};
            bool decoupledPitch{false};
            bool pawnControl{true};
        } internalSettings{};
        DebugData debugData{};
    };

    extern Settings gStore;

//    inline int gameLoopFrameCount() {
//        // 1.14.74
//        static REL::Relocation<int*> gameFc{(uintptr_t)MemoryScan::mod + 0x6101e3c};
//        return *gameFc;
//    }

    inline int renderLoopFrameCount() {
        static REL::Relocation<int*> globalFrameCountAddr{ GameStore::MemoryOffsets::CreationRenderer::GlobalFrameCount() };
//        static REL::Relocation<int*> renderFc{(uintptr_t)MemoryScan::mod + 0x6a50260};
        return *globalFrameCountAddr;
    }
}

