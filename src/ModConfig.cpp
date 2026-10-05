#include "ModConfig.h"
#include "Mods.hpp"
#include <mods/VR.hpp>
#include <mods/VRConfig.hpp>
#ifdef _DEBUG
#include <nvidia/ShaderDebugOverlay.h>
#endif
#include <nvidia/UpscalerAfrNvidiaModule.h>

#include "ModSettings.h"
#include "CreationEngine/CreationEngineEntry.h"
#include "CreationEngine/VROptions.h"
#include "CreationEngine/GameSettingsComponent.h"
#include "CreationEngine/models/ModSettingsStore.h"
#include "CreationEngine/models/GameFlow.h"

namespace ModSettings {
    // HudScale g_hudScale;
    // CrosshairTranslate g_crosshairTranslate;
    // AW2InternalSettings g_aw2_settings;
    // AW2GameFlow g_game_state;
    // DebugAndCalibration g_debugAndCalibration;
}

Mods::Mods() {
    m_mods.emplace_back(VRConfig::get());
    m_mods.emplace_back(VR::get());
    m_mods.emplace_back(UpscalerAfrNvidiaModule::Get());
#ifdef _DEBUG
    m_mods.emplace_back(ShaderDebugOverlay::Get());
#endif
    m_mods.emplace_back(CreationEngineEntry::Get());
    m_mods.emplace_back(VROptions::Get());
    m_mods.emplace_back(GameSettingsComponent::Get());
}




void VR::on_xinput_get_state(uint32_t* retval, uint32_t user_index, XINPUT_STATE* state) {
//    ZoneScopedN(__FUNCTION__);
    auto pXinputGamepad = (XINPUT_GAMEPAD*)state;

    const auto now = std::chrono::steady_clock::now();

    if (now - m_last_xinput_update > std::chrono::seconds(2)) {
        m_lowest_xinput_user_index = user_index;
    }

    if (user_index < m_lowest_xinput_user_index) {
        m_lowest_xinput_user_index = user_index;
        spdlog::info("[VR] Changed lowest XInput user index to {}", user_index);
    }

    if (user_index != m_lowest_xinput_user_index) {
        if (!m_spoofed_gamepad_connection && is_using_controllers()) {
            spdlog::info("[VR] XInputGetState called, but user index is {}", user_index);
        }

        return;
    }

    if (!m_spoofed_gamepad_connection) {
        spdlog::info("[VR] Successfully spoofed gamepad connection @ {}", user_index);
    }

    m_last_xinput_update = now;
    m_spoofed_gamepad_connection = true;

    if (is_using_controllers_within(std::chrono::minutes(5)))
        if (is_using_controllers_within(std::chrono::minutes(5)))
        {
            *retval = ERROR_SUCCESS;

        }

    if (!is_using_controllers()) {
        set_comfort_vignette(0.0f);
        set_comfort_fade(0.0f);
        return;
    }

//    // Clear button state for VR controllers
//    if (is_using_controllers_within(std::chrono::seconds(5))) {
//        state->Gamepad.wButtons = 0;
//        state->Gamepad.bLeftTrigger = 0;
//        state->Gamepad.bRightTrigger = 0;
//        state->Gamepad.sThumbLX = 0;
//        state->Gamepad.sThumbLY = 0;
//        state->Gamepad.sThumbRX = 0;
//        state->Gamepad.sThumbRY = 0;
//    }


    const auto wants_swap = m_swap_controllers->value();
    const auto left_joystick = !wants_swap ? get_left_joystick():get_right_joystick();
    const auto right_joystick = !wants_swap? get_right_joystick():get_left_joystick();

    const auto& a_button_left = !wants_swap ? m_action_a_button_left : m_action_a_button_right;
    const auto& a_button_right = !wants_swap ? m_action_a_button_right : m_action_a_button_left;

    const auto is_right_a_button_down = is_action_active_any_joystick(a_button_right);
    const auto is_left_a_button_down = is_action_active_any_joystick(a_button_left);

    const auto is_left_joystick_click_down = is_action_active(m_action_joystick_click, left_joystick);
    const auto is_right_joystick_click_down = is_action_active(m_action_joystick_click, right_joystick);

    if (g_framework->is_drawing_ui()) {
        set_comfort_vignette(0.0f);
        set_comfort_fade(0.0f);
        return;
    }

    static std::chrono::steady_clock::time_point last_snap_fade{};

    // Snap turn: a right-stick flick queues a yaw step; menus and ship piloting keep the raw stick.
    const auto& internal_settings = GameFlow::gStore.internalSettings;
    const bool snap_turn_active = internal_settings.turnMode == 0 && !GameFlow::isShowingMenu() && !GameFlow::isPilotingShip();
    if (snap_turn_active) {
        constexpr float kSnapTrigger = 0.7f;
        constexpr float kSnapRearm   = 0.3f;
        constexpr auto  kSnapRepeat  = std::chrono::milliseconds(300);

        static bool                                  snap_armed{true};
        static std::chrono::steady_clock::time_point last_snap{};

        const float x = get_joystick_axis(right_joystick).x;
        if (std::abs(x) < kSnapRearm) {
            snap_armed = true;
        } else if (snap_armed && std::abs(x) >= kSnapTrigger && now - last_snap >= kSnapRepeat) {
            snap_armed = false;
            last_snap  = now;
            const float step = glm::radians(internal_settings.snapTurnDegrees) * (x > 0.0f ? 1.0f : -1.0f);
            GameFlow::pendingSnapYaw.fetch_add(step);
            if (internal_settings.turnFade) {
                last_snap_fade = now;
            }
        }
    }

    // Black long enough for the turn to land, then fade back in.
    {
        constexpr float kHoldSeconds = 0.05f;
        constexpr float kFadeSeconds = 0.12f;
        const float since = std::chrono::duration<float>(now - last_snap_fade).count();
        const float fade = since < kHoldSeconds ? 1.0f : std::clamp(1.0f - (since - kHoldSeconds) / kFadeSeconds, 0.0f, 1.0f);
        set_comfort_fade(fade);
    }

    // Comfort shaping of the sticks, on foot only.
    const bool on_foot = !GameFlow::isShowingMenu() && !GameFlow::isPilotingShip();
    const bool block_sprint = on_foot && internal_settings.speedLimit < 1.0f;

    Vector2f look_axis = get_joystick_axis(right_joystick);
    if (on_foot) {
        look_axis.x *= internal_settings.smoothTurnSpeed;
        if (!internal_settings.stickPitch) {
            look_axis.y = 0.0f;
        }
    }

    const Vector2f move_axis = [&] {
        Vector2f axis = get_joystick_axis(left_joystick);
        if (!on_foot) {
            return axis;
        }
        if (internal_settings.moveDirection == 1) {
            const auto yaw_of = [](const Matrix4x4f& m) {
                const auto forward = -Vector3f{ m[2] };
                return std::atan2(forward.x, -forward.z);
            };
            const float delta = yaw_of(get_transform(get_left_controller_index())) - yaw_of(get_transform(0));
            const float c = std::cos(delta), s = std::sin(delta);
            axis = Vector2f{ axis.x * c + axis.y * s, -axis.x * s + axis.y * c };
        }

        // Ease the magnitude toward its target and keep the last direction while decelerating.
        static Vector2f                              last_direction{ 0.0f, 1.0f };
        static float                                 shaped_magnitude{ 0.0f };
        static std::chrono::steady_clock::time_point last_update{ now };

        const float magnitude = std::min(glm::length(axis), 1.0f);
        if (magnitude > 0.05f) {
            last_direction = axis / glm::length(axis);
        }
        const float target = std::min(magnitude, internal_settings.speedLimit);
        const float dt     = std::min(std::chrono::duration<float>(now - last_update).count(), 0.1f);
        last_update        = now;

        if (internal_settings.smoothAcceleration) {
            constexpr float kAccelPerSecond = 1.0f / 0.4f;
            constexpr float kDecelPerSecond = 1.0f / 0.15f;
            const float rate = target > shaped_magnitude ? kAccelPerSecond : kDecelPerSecond;
            const float step = rate * dt;
            shaped_magnitude = std::abs(target - shaped_magnitude) <= step ? target : shaped_magnitude + (target > shaped_magnitude ? step : -step);
        } else {
            shaped_magnitude = target;
        }
        return last_direction * shaped_magnitude;
    }();

    // Vignette while moving or turning smoothly on foot.
    {
        static float                                 vignette{ 0.0f };
        static std::chrono::steady_clock::time_point last_update{ now };
        const float dt = std::min(std::chrono::duration<float>(now - last_update).count(), 0.1f);
        last_update    = now;

        float activity = on_foot ? glm::length(move_axis) : 0.0f;
        if (on_foot && !snap_turn_active) {
            activity = std::max(activity, std::abs(look_axis.x));
        }

        // Jump and jetpack (gamepad A): held, plus a short tail to cover the airtime.
        const auto& jump_action = internal_settings.controllerLayout == 0 || !wants_swap ? m_action_a_button_right : m_action_a_button_left;
        static std::chrono::steady_clock::time_point last_jump{};
        if (on_foot && is_action_active_any_joystick(jump_action)) {
            last_jump = now;
        }
        if (on_foot && now - last_jump < std::chrono::milliseconds(900)) {
            activity = 1.0f;
        }
        const float t      = std::clamp((activity - 0.1f) / 0.4f, 0.0f, 1.0f);
        const float target = internal_settings.vignetteStrength * t * t * (3.0f - 2.0f * t);
        const float step   = (target > vignette ? 1.0f / 0.15f : 1.0f / 0.3f) * dt;
        vignette           = std::abs(target - vignette) <= step ? target : vignette + (target > vignette ? step : -step);
        set_comfort_vignette(vignette);
    }

    // Matching letters: each button sends the gamepad button with the same label.
    if (GameFlow::gStore.internalSettings.controllerLayout == 0) {
        using clock = std::chrono::steady_clock;
        constexpr auto kTapPulse = std::chrono::milliseconds(100);
        constexpr auto kMenuHold = std::chrono::milliseconds(500);
        constexpr float kStickDeflection = 0.5f;

        auto& buttons = pXinputGamepad->wButtons;

        if (is_action_active_any_joystick(m_action_a_button_right)) buttons |= XINPUT_GAMEPAD_A;
        if (is_action_active_any_joystick(m_action_b_button_right)) buttons |= XINPUT_GAMEPAD_B;
        if (is_action_active_any_joystick(m_action_a_button_left))  buttons |= XINPUT_GAMEPAD_X;
        if (is_action_active_any_joystick(m_action_b_button_left))  buttons |= XINPUT_GAMEPAD_Y;

        if (is_left_joystick_click_down)  buttons |= XINPUT_GAMEPAD_LEFT_THUMB;
        if (is_right_joystick_click_down) buttons |= XINPUT_GAMEPAD_RIGHT_THUMB;

        const auto left_trigger_down  = is_action_active(m_action_trigger, left_joystick);
        const auto right_trigger_down = is_action_active(m_action_trigger, right_joystick);
        const auto left_grip_down     = is_action_active(m_action_grip, left_joystick);
        const auto right_grip_down    = is_action_active(m_action_grip, right_joystick);

        if (left_trigger_down)  pXinputGamepad->bLeftTrigger  = 255;
        if (right_trigger_down) pXinputGamepad->bRightTrigger = 255;
        if (right_grip_down)    buttons |= XINPUT_GAMEPAD_RIGHT_SHOULDER;

        const auto now = clock::now();
        const auto left_axis  = get_joystick_axis(left_joystick);

        // Left grip: held with the left stick it is a D-pad; a tap with no stick use sends LB on release.
        static bool              left_grip_was_down{false};
        static bool              left_grip_used_as_modifier{false};
        static clock::time_point lb_pulse_until{};

        if (left_grip_down) {
            if (std::abs(left_axis.x) >= kStickDeflection || std::abs(left_axis.y) >= kStickDeflection || is_left_joystick_click_down) {
                left_grip_used_as_modifier = true;
            }
            if (left_axis.y >= kStickDeflection)  buttons |= XINPUT_GAMEPAD_DPAD_UP;
            if (left_axis.y <= -kStickDeflection) buttons |= XINPUT_GAMEPAD_DPAD_DOWN;
            if (left_axis.x >= kStickDeflection)  buttons |= XINPUT_GAMEPAD_DPAD_RIGHT;
            if (left_axis.x <= -kStickDeflection) buttons |= XINPUT_GAMEPAD_DPAD_LEFT;

            if (is_left_joystick_click_down) {
                // Left grip + left stick click toggles the flat-screen view.
                buttons &= ~XINPUT_GAMEPAD_LEFT_THUMB;
                static clock::time_point last_flat_toggle{};
                if (now - last_flat_toggle > kMenuHold) {
                    last_flat_toggle = now;
                    auto& flat_screen = ModSettings::g_internalSettings.forceFlatScreen;
                    flat_screen = !flat_screen;
                }
            }
        } else if (left_grip_was_down) {
            if (!left_grip_used_as_modifier) {
                lb_pulse_until = now + kTapPulse;
            }
            left_grip_used_as_modifier = false;
        }
        left_grip_was_down = left_grip_down;

        if (now < lb_pulse_until) buttons |= XINPUT_GAMEPAD_LEFT_SHOULDER;

        // Menu button: tap = Start (pause), hold = View/Back (data menus).
        static bool              menu_was_down{false};
        static clock::time_point menu_down_since{};
        static clock::time_point start_pulse_until{};

        const auto menu_down = is_action_active_any_joystick(m_action_system_button);
        if (menu_down) {
            if (!menu_was_down) {
                menu_down_since = now;
            }
            if (now - menu_down_since >= kMenuHold) {
                buttons |= XINPUT_GAMEPAD_BACK;
            }
        } else if (menu_was_down && now - menu_down_since < kMenuHold) {
            start_pulse_until = now + kTapPulse;
        }
        menu_was_down = menu_down;

        if (now < start_pulse_until) buttons |= XINPUT_GAMEPAD_START;

        if (is_action_active_any_joystick(m_action_dpad_up))    buttons |= XINPUT_GAMEPAD_DPAD_UP;
        if (is_action_active_any_joystick(m_action_dpad_right)) buttons |= XINPUT_GAMEPAD_DPAD_RIGHT;
        if (is_action_active_any_joystick(m_action_dpad_down))  buttons |= XINPUT_GAMEPAD_DPAD_DOWN;
        if (is_action_active_any_joystick(m_action_dpad_left))  buttons |= XINPUT_GAMEPAD_DPAD_LEFT;

        // The left stick is a D-pad while the left grip is held.
        if (!left_grip_down) {
            pXinputGamepad->sThumbLX = (int16_t)std::clamp<float>((float)pXinputGamepad->sThumbLX + move_axis.x * 32767.0f, -32767.0f, 32767.0f);
            pXinputGamepad->sThumbLY = (int16_t)std::clamp<float>((float)pXinputGamepad->sThumbLY + move_axis.y * 32767.0f, -32767.0f, 32767.0f);
        }
        if (!snap_turn_active) {
            pXinputGamepad->sThumbRX = (int16_t)std::clamp<float>((float)pXinputGamepad->sThumbRX + look_axis.x * 32767.0f, -32767.0f, 32767.0f);
        }
        pXinputGamepad->sThumbRY = (int16_t)std::clamp<float>((float)pXinputGamepad->sThumbRY + look_axis.y * 32767.0f, -32767.0f, 32767.0f);
        if (block_sprint && !left_grip_down) {
            buttons &= ~XINPUT_GAMEPAD_LEFT_THUMB;
        }
        return;
    }

    if (is_right_a_button_down) {
        pXinputGamepad->wButtons |= XINPUT_GAMEPAD_A;
    }

    if (is_left_a_button_down) {
        pXinputGamepad->wButtons |= XINPUT_GAMEPAD_B;
    }

    const auto& b_button_left = !wants_swap ? m_action_b_button_left : m_action_b_button_right;
    const auto& b_button_right = !wants_swap ? m_action_b_button_right : m_action_b_button_left;

    const auto is_right_b_button_down = is_action_active_any_joystick(b_button_right);
    const auto is_left_b_button_down = is_action_active_any_joystick(b_button_left);

    if (is_right_b_button_down) {
        pXinputGamepad->wButtons |= XINPUT_GAMEPAD_Y;
    }

    if (is_left_b_button_down) {
        pXinputGamepad->wButtons |= XINPUT_GAMEPAD_X;
    }


    {
        if (is_left_joystick_click_down) {
            pXinputGamepad->wButtons |= XINPUT_GAMEPAD_LEFT_THUMB;
        }

        if (is_right_joystick_click_down) {
            pXinputGamepad->wButtons |= XINPUT_GAMEPAD_RIGHT_THUMB;
        }
    }


    const auto is_left_trigger_down = is_action_active(m_action_trigger, left_joystick);
    const auto is_right_trigger_down = is_action_active(m_action_trigger, right_joystick);
    const auto is_left_grip_down = is_action_active(m_action_grip, left_joystick);
    const auto is_right_grip_down = is_action_active(m_action_grip, right_joystick);


    if((is_left_joystick_click_down && is_left_grip_down) || (GetAsyncKeyState(VK_F12) & 1)) {
        static std::chrono::steady_clock::time_point m_last_grip_click = std::chrono::steady_clock::now();
        if(std::chrono::steady_clock::now() - m_last_grip_click > std::chrono::milliseconds(500)) {
            m_last_grip_click                           = std::chrono::steady_clock::now();
            auto& flatScreen = ModSettings::g_internalSettings.forceFlatScreen;
            flatScreen = !flatScreen;
        }
    }

    const auto system_button_down = is_action_active_any_joystick(m_action_system_button);
    if(system_button_down) {
        if(is_left_grip_down) {
            pXinputGamepad->wButtons |= XINPUT_GAMEPAD_BACK;
            pXinputGamepad->wButtons &= ~XINPUT_GAMEPAD_START;
        } else {
            pXinputGamepad->wButtons |= XINPUT_GAMEPAD_START;
            pXinputGamepad->wButtons &= ~XINPUT_GAMEPAD_BACK;
        }

    } else {
        pXinputGamepad->wButtons &= ~XINPUT_GAMEPAD_START;
        pXinputGamepad->wButtons &= ~XINPUT_GAMEPAD_BACK;
    }

    if (is_right_grip_down) {
        pXinputGamepad->bLeftTrigger = 255;
    }

    if (is_right_trigger_down) {
        pXinputGamepad->bRightTrigger = 255;
    }

    const auto thumbrest_touch_right_down = is_action_active_any_joystick(m_action_thumbrest_touch_right);
    const auto thumbrest_touch_left_down = is_action_active_any_joystick(m_action_thumbrest_touch_left);


    if (thumbrest_touch_right_down && !GameFlow::gStore.internalSettings.alternativeJoyLayout) {
        pXinputGamepad->wButtons |= XINPUT_GAMEPAD_RIGHT_SHOULDER;
    } else if(is_left_grip_down && is_right_trigger_down && GameFlow::gStore.internalSettings.alternativeJoyLayout) {
        pXinputGamepad->wButtons |= XINPUT_GAMEPAD_RIGHT_SHOULDER;
        pXinputGamepad->bRightTrigger = 0;
    }

    if (thumbrest_touch_left_down && !GameFlow::gStore.internalSettings.alternativeJoyLayout) {
        pXinputGamepad->wButtons |= XINPUT_GAMEPAD_LEFT_SHOULDER;
    } else if(is_left_grip_down && is_left_trigger_down && GameFlow::gStore.internalSettings.alternativeJoyLayout) {
        pXinputGamepad->wButtons |= XINPUT_GAMEPAD_LEFT_SHOULDER;
    }

    const auto is_dpad_up_down = is_action_active_any_joystick(m_action_dpad_up);

    if (is_dpad_up_down) {
        pXinputGamepad->wButtons |= XINPUT_GAMEPAD_DPAD_UP;
    }

    const auto is_dpad_right_down = is_action_active_any_joystick(m_action_dpad_right);

    if (is_dpad_right_down) {
        pXinputGamepad->wButtons |= XINPUT_GAMEPAD_DPAD_RIGHT;
    }

    const auto is_dpad_down_down = is_action_active_any_joystick(m_action_dpad_down);

    if (is_dpad_down_down) {
        pXinputGamepad->wButtons |= XINPUT_GAMEPAD_DPAD_DOWN;
    }

    const auto is_dpad_left_down = is_action_active_any_joystick(m_action_dpad_left);

    if (is_dpad_left_down) {
        pXinputGamepad->wButtons |= XINPUT_GAMEPAD_DPAD_LEFT;
    }

    const auto true_left_joystick_axis = get_joystick_axis(m_left_joystick);
    const auto true_right_joystick_axis = get_joystick_axis(m_right_joystick);

    pXinputGamepad->sThumbLX = (int16_t)std::clamp<float>(((float)pXinputGamepad->sThumbLX + move_axis.x * 32767.0f), -32767.0f, 32767.0f);
    pXinputGamepad->sThumbLY = (int16_t)std::clamp<float>(((float)pXinputGamepad->sThumbLY + move_axis.y * 32767.0f), -32767.0f, 32767.0f);

    if (!snap_turn_active) {
        pXinputGamepad->sThumbRX = (int16_t)std::clamp<float>(((float)pXinputGamepad->sThumbRX + look_axis.x * 32767.0f), -32767.0f, 32767.0f);
    }
    pXinputGamepad->sThumbRY = (int16_t)std::clamp<float>(((float)pXinputGamepad->sThumbRY + look_axis.y * 32767.0f), -32767.0f, 32767.0f);
    if (block_sprint && !is_left_grip_down) {
        pXinputGamepad->wButtons &= ~XINPUT_GAMEPAD_LEFT_THUMB;
    }


    // Touching the thumbrest allows us to use the thumbstick as a dpad.  Additional options are for controllers without capacitives/games that rely solely on DPad
    if (is_left_grip_down) {

        float ty{0.0f};
        float tx{0.0f};
        //SHORT ThumbY{0};
        //SHORT ThumbX{0};
        // If someone is accidentally touching both thumbrests while also moving a joystick, this will default to left joystick.

        ty = true_left_joystick_axis.y; // ? wants_swap
        tx = true_left_joystick_axis.x;

        if (ty >= 0.5f) {
            pXinputGamepad->wButtons |= XINPUT_GAMEPAD_DPAD_UP;
        }

        if (ty <= -0.5f) {
            pXinputGamepad->wButtons |= XINPUT_GAMEPAD_DPAD_DOWN;
        }

        if (tx >= 0.5f) {
            pXinputGamepad->wButtons |= XINPUT_GAMEPAD_DPAD_RIGHT;
        }

        if (tx <= -0.5f) {
            pXinputGamepad->wButtons |= XINPUT_GAMEPAD_DPAD_LEFT;
        }

        if (wants_swap) {
            pXinputGamepad->sThumbRY = 0;
            pXinputGamepad->sThumbRX = 0;
        } else {
            pXinputGamepad->sThumbLY = 0;
            pXinputGamepad->sThumbLX = 0;
        }
    }

}

