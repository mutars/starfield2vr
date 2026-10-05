#pragma once
#include "glm/glm.hpp"
#include <atomic>

namespace GameFlow {
    // Snap-turn yaw (radians) requested by the input thread, applied once by the camera hook.
    inline std::atomic<float> pendingSnapYaw{0.0f};

    struct MenuSettings
    {
        float hud_scale;
        int   perspective;
    };
    struct State
    {

        struct DebugWeaponData {
            int deepLevel;
            bool type;
            float yaw{0.f}, pitch{0.f}, roll {0.f};
        } debugWeaponData{};
        struct UiData {
            int rendered_menus_count[2]{};
            int modulino{0};
        } uiData{};

        struct RayCastData {
            glm::vec3 weaponRayCastOrigin{};
            glm::vec3 weaponRayCastDestination{};
        } rayCastData{};
    };

    extern State gState;

    void renderMenu(std::string_view menuNameHash);
    MenuSettings getMenuSettings(std::string_view menuNameHash);
    bool isShowingMenu();
    bool isPilotingShip();
    bool isAimingDownSights();
    bool isWeaponDrawn();
    bool isImmovable();
    bool isControlledByAI();
    bool isInFirstPerson();
    void resetGameState();
}

