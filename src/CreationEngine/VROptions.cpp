#include "VROptions.h"

#include "CreationEngineConstants.h"
#include "StereoViewModule.h"
#include "Framework.hpp"
#include <CreationEngine/models/ModSettingsStore.h>
#include <mods/VR.hpp>

namespace
{
    constexpr std::array<float, 4> kSnapAngles{ 30.0f, 45.0f, 60.0f, 90.0f };
    constexpr std::array<float, 3> kSmoothTurnSpeeds{ 0.4f, 0.7f, 1.0f };
    constexpr std::array<float, 3> kSpeedLimits{ 1.0f, 0.75f, 0.45f };
    constexpr std::array<float, 4> kVignetteStrengths{ 0.0f, 0.35f, 0.6f, 0.85f };
    constexpr std::array<float, 8> kRenderResolutions{ 0.7f, 0.8f, 0.9f, 1.0f, 1.1f, 1.2f, 1.3f, 1.5f };
    constexpr std::array<float, 5> kWorldScales{ 0.9f, 0.95f, 1.0f, 1.05f, 1.1f };
    constexpr std::array<float, 3> kHudSizes{ 0.3f, 0.4f, 0.5f };
    constexpr std::array<int, 3>   kHudDepths{ 100, 150, 250 };
    constexpr std::array<float, 3> kMenuDistances{ 1.5f, 2.0f, 3.0f };
    constexpr std::array<float, 3> kMenuSizes{ 1.5f, 2.0f, 2.5f };

    template <class T, std::size_t N>
    T Pick(const std::array<T, N>& values, int index)
    {
        return values[std::clamp(index, 0, (int)N - 1)];
    }
}

const std::array<VROptions::CategoryInfo, 3>& VROptions::categories()
{
    static const std::array<CategoryInfo, 3> list{ {
        { kCategoryComfort, "VR Comfort" },
        { kCategoryDisplay, "VR Display" },
        { kCategoryControls, "VR Controls" },
    } };
    return list;
}

VROptions::VROptions()
{
    auto add = [this](Id id, Category category, Kind kind, const char* key, const char* label, const char* description,
                   std::vector<std::string> choices, int default_value, std::array<int, 3> preset = { -1, -1, -1 }) {
        Option option{ id, category, kind, label, description, choices, preset, nullptr };
        if (kind == Kind::Toggle || kind == Kind::Choice) {
            option.value = ModCombo::create(generate_name(key), std::move(choices), default_value);
        }
        m_options.push_back(std::move(option));
    };
    const std::vector<std::string> on_off{ "Off", "On" };

    // VR Comfort
    add(kComfortPreset, kCategoryComfort, Kind::Preset, "", "Comfort Preset",
        "Sets the comfort options below. Low suits players used to VR; High is gentlest for those prone to motion sickness.",
        { "Low", "Medium", "High", "Custom" }, 0);
    add(kSnapTurn, kCategoryComfort, Kind::Toggle, "SnapTurn", "Snap Turn", "Turn in fixed steps with the right stick instead of smoothly.",
        on_off, 1, { 0, 1, 1 });
    add(kSnapTurnAngle, kCategoryComfort, Kind::Choice, "SnapTurnAngle", "Snap Turn Angle", "How far each snap turn rotates you.",
        { "30\xC2\xB0", "45\xC2\xB0", "60\xC2\xB0", "90\xC2\xB0" }, 1);
    add(kTurnFade, kCategoryComfort, Kind::Toggle, "TurnFade", "Turn Fade", "Briefly fade to black on each snap turn so it reads as a cut.",
        on_off, 0, { 0, 0, 1 });
    add(kSmoothTurnSpeed, kCategoryComfort, Kind::Choice, "SmoothTurnSpeed", "Smooth Turn Speed",
        "How fast the right stick turns you when Snap Turn is off.", { "Slow", "Medium", "Fast" }, 1);
    add(kStickPitch, kCategoryComfort, Kind::Toggle, "StickPitch", "Stick Looks Up and Down",
        "Let the right stick tilt your view up and down. When off, only your head does.", on_off, 0, { 1, 0, 0 });
    add(kSpeedLimit, kCategoryComfort, Kind::Choice, "SpeedLimit", "Speed Limit", "Cap how fast the left stick moves you. Sprinting is off below Full.",
        { "Full", "Jog", "Walk" }, 0, { 0, 0, 1 });
    add(kSmoothAcceleration, kCategoryComfort, Kind::Toggle, "SmoothAcceleration", "Smooth Acceleration",
        "Ease into and out of movement instead of starting and stopping instantly.", on_off, 1, { 0, 1, 1 });
    add(kVignette, kCategoryComfort, Kind::Choice, "Vignette", "Vignette", "Darken the edges of your view while you move or turn smoothly.",
        { "Off", "Light", "Medium", "Strong" }, 2, { 0, 2, 3 });
    add(kAimZoom, kCategoryComfort, Kind::Toggle, "AimZoom", "Aim Zoom", "Let the game zoom your view when aiming down sights.", on_off, 1,
        { 1, 1, 0 });

    // VR Display
    add(kRenderResolution, kCategoryDisplay, Kind::Choice, "RenderResolution", "Render Resolution",
        "Resolution rendered for the headset. Lower it if the frame rate drops.",
        { "70%", "80%", "90%", "100%", "110%", "120%", "130%", "150%" }, 3);
    add(kWorldScale, kCategoryDisplay, Kind::Choice, "WorldScale", "World Scale", "Make the world feel larger or smaller around you.",
        { "90%", "95%", "100%", "105%", "110%" }, 2);
    add(kMatchBodyHeight, kCategoryDisplay, Kind::Toggle, "MatchBodyHeight", "Match Body Height",
        "Scale your movement so your eyes and reach match your character's body. Measured when the view recenters.", on_off, 1);
    add(kHudPlacement, kCategoryDisplay, Kind::Choice, "HudPlacement", "HUD Placement",
        "Floating Panel shows the HUD on a panel in front of you that recentres when you turn away. Head Locked keeps it fixed in your view.",
        { "Floating Panel", "Head Locked" }, 0);
    add(kHudSize, kCategoryDisplay, Kind::Choice, "HudSize", "HUD Size", "Size of the in-game HUD.", { "Small", "Medium", "Large" }, 1);
    add(kHudDepth, kCategoryDisplay, Kind::Choice, "HudDepth", "HUD Depth", "How far away the in-game HUD appears.", { "Near", "Medium", "Far" }, 1);
    add(kMenuDistance, kCategoryDisplay, Kind::Choice, "MenuDistance", "Menu Distance", "How far away full-screen menus appear.",
        { "Near", "Medium", "Far" }, 1);
    add(kMenuSize, kCategoryDisplay, Kind::Choice, "MenuSize", "Menu Size", "Size of full-screen menus.", { "Small", "Medium", "Large" }, 1);
    add(kRecenterAfterLoading, kCategoryDisplay, Kind::Toggle, "RecenterAfterLoading", "Recenter After Loading Screens",
        "Reset your view height and direction when a loading screen ends.", on_off, 1);
    add(kRecenterView, kCategoryDisplay, Kind::Action, "", "Recenter View", "Reset your view height and direction now.", {}, 0);
    add(kEyeScreenshots, kCategoryDisplay, Kind::Action, "", "Save Eye Screenshots",
        "Save the image each eye of the headset receives, as PNG files in the game folder. Useful when reporting a display problem.", {}, 0);

    // VR Controls
    add(kControllerLayout, kCategoryControls, Kind::Choice, "ControllerLayout", "Controller Layout",
        "Matching letters: each controller button sends the gamepad button with the same label, so on-screen prompts match.",
        { "Matching letters", "Legacy" }, 0);
    add(kMoveDirection, kCategoryControls, Kind::Choice, "MoveDirection", "Movement Direction", "Move toward where your head or your left hand points.",
        { "Head", "Left Hand" }, 1);
    add(kAimWith, kCategoryControls, Kind::Choice, "AimWith", "Aim With", "Aim with your head, or point your weapon with your right hand.",
        { "Head when aiming", "Free", "Always head", "Right Hand" }, 0);
    add(kBodyLean, kCategoryControls, Kind::Toggle, "BodyLean", "Body Lean",
        "Bend the body's spine when you lean, keeping the hips in place.", on_off, 1);
    add(kBodyFacing, kCategoryControls, Kind::Toggle, "BodyFacing", "Body Faces Hands",
        "Turn the torso partly toward your hands instead of only following your head.", on_off, 1);
    add(kSupportHand, kCategoryControls, Kind::Toggle, "SupportHand", "Two-Handed Grip",
        "Your left hand holds the weapon's foregrip when you bring it close.", on_off, 1);
    add(kFingerPoses, kCategoryControls, Kind::Toggle, "FingerPoses", "Finger Tracking",
        "Fingers follow the controller's trigger, grip and thumb touch.", on_off, 1);
    add(kBodyTracking, kCategoryControls, Kind::Toggle, "BodyTracking", "Body Tracking",
        "Use the headset's body tracking for your torso, elbows and legs when available.", on_off, 1);
    add(kHandTracking, kCategoryControls, Kind::Toggle, "HandTracking", "Tracked Fingers",
        "Use the headset's hand tracking for finger poses when available.", on_off, 1);
    add(kWalkingLegs, kCategoryControls, Kind::Toggle, "WalkingLegs", "Walking Legs",
        "The body's legs step whenever you move.", on_off, 1);
    add(kWeaponFollowsHand, kCategoryControls, Kind::Toggle, "WeaponFollowsHand", "Full Body (Experimental)",
        "Show your character's body in first person. With Aim With set to Right Hand, your right hand and weapon follow the controller.", on_off, 0);
}

const VROptions::Option* VROptions::find(std::uint32_t id) const
{
    for (auto& option : m_options) {
        if (option.id == id) {
            return &option;
        }
    }
    return nullptr;
}

int VROptions::get(std::uint32_t id) const
{
    if (id == kComfortPreset) {
        return current_preset();
    }
    auto option = find(id);
    return option && option->value ? option->value->value() : 0;
}

int VROptions::current_preset() const
{
    for (int preset = 0; preset < kCustomPreset; ++preset) {
        bool matches = true;
        for (auto& option : m_options) {
            if (option.value && option.preset[preset] >= 0 && option.value->value() != option.preset[preset]) {
                matches = false;
                break;
            }
        }
        if (matches) {
            return preset;
        }
    }
    return kCustomPreset;
}

void VROptions::apply_preset(int preset)
{
    for (auto& option : m_options) {
        if (option.value && option.preset[preset] >= 0) {
            option.value->value() = option.preset[preset];
        }
    }
}

void VROptions::set(std::uint32_t id, int value)
{
    if (id == kRecenterView) {
        VR::get()->recenter_view();
        return;
    }
    if (id == kEyeScreenshots) {
        StereoViewModule::Get()->RequestEyeScreenshots();
        return;
    }
    if (id == kComfortPreset) {
        if (value >= 0 && value < kCustomPreset) {
            apply_preset(value);
        }
    } else if (auto option = find(id); option && option->value) {
        option->value->value() = std::clamp<int>(value, 0, (int)option->choices.size() - 1);
    } else {
        return;
    }
    apply();
    g_framework->request_save_config();
}

void VROptions::apply() const
{
    auto& s                = GameFlow::gStore.internalSettings;
    s.turnMode             = get(kSnapTurn) ? 0 : 1;
    s.snapTurnDegrees      = Pick(kSnapAngles, get(kSnapTurnAngle));
    s.turnFade             = get(kTurnFade) != 0;
    s.smoothTurnSpeed      = Pick(kSmoothTurnSpeeds, get(kSmoothTurnSpeed));
    s.stickPitch           = get(kStickPitch) != 0;
    s.moveDirection        = get(kMoveDirection);
    s.speedLimit           = Pick(kSpeedLimits, get(kSpeedLimit));
    s.smoothAcceleration   = get(kSmoothAcceleration) != 0;
    s.vignetteStrength     = Pick(kVignetteStrengths, get(kVignette));
    s.preventZoom          = get(kAimZoom) == 0;
    s.controllerLayout     = get(kControllerLayout);
    s.weaponFollowsHand    = get(kWeaponFollowsHand) != 0;
    s.recenterAfterLoading = get(kRecenterAfterLoading) != 0;
    s.matchBodyHeight      = get(kMatchBodyHeight) != 0;
    s.bodyLean             = get(kBodyLean) != 0;
    s.bodyFacing           = get(kBodyFacing) != 0;
    s.supportHand          = get(kSupportHand) != 0;
    s.fingerPoses          = get(kFingerPoses) != 0;
    s.walkingLegs          = get(kWalkingLegs) != 0;
    s.bodyTracking         = get(kBodyTracking) != 0;
    s.handTracking         = get(kHandTracking) != 0;
    s.hudPanel             = get(kHudPlacement) == 0;

    auto& hud       = GameFlow::gStore.hudSettings;
    hud.hudScale    = Pick(kHudSizes, get(kHudSize));
    hud.perspective = Pick(kHudDepths, get(kHudDepth));
    // The floating panel spans 40, 50 or 60 degrees at 1.2, 2 or 3 metres.
    constexpr std::array<float, 3> kHudPanelAngles{ 40.0f, 50.0f, 60.0f };
    constexpr std::array<float, 3> kHudPanelDistances{ 1.2f, 2.0f, 3.0f };
    s.hudPanelDistance = Pick(kHudPanelDistances, get(kHudDepth));
    s.hudPanelWidth    = 2.0f * s.hudPanelDistance * std::tan(glm::radians(Pick(kHudPanelAngles, get(kHudSize))) * 0.5f);

    ModConstants::headTrackingType = get(kAimWith);

    auto vr = VR::get();
    vr->set_resolution_scale(Pick(kRenderResolutions, get(kRenderResolution)));
    vr->set_world_scale(Pick(kWorldScales, get(kWorldScale)));
    vr->get_overlay_component().set_slate(Pick(kMenuDistances, get(kMenuDistance)), Pick(kMenuSizes, get(kMenuSize)));
}

void VROptions::on_config_load(const utility::Config& cfg, bool set_defaults)
{
    for (auto& option : m_options) {
        if (option.value) {
            option.value->config_load(cfg, set_defaults);
        }
    }
    apply();
}

void VROptions::on_config_save(utility::Config& cfg)
{
    for (auto& option : m_options) {
        if (option.value) {
            option.value->config_save(cfg);
        }
    }
}
