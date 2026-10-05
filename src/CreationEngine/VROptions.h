#pragma once
#include <array>
#include <Mod.hpp>

// Player-facing VR options, shown as VR categories in the game's Settings menu.
class VROptions : public Mod
{
public:
    enum class Kind
    {
        Toggle,
        Choice,
        Preset,
        Action,
    };

    // Category IDs 0-6 are the game's.
    enum Category : std::uint32_t
    {
        kCategoryComfort  = 86,
        kCategoryDisplay  = 87,
        kCategoryControls = 88,
    };

    enum Id : std::uint32_t
    {
        kComfortPreset = 8600,
        kSnapTurn,
        kSnapTurnAngle,
        kRecenterAfterLoading,
        kControllerLayout,
        kSmoothTurnSpeed,
        kStickPitch,
        kMoveDirection,
        kSpeedLimit,
        kSmoothAcceleration,
        kVignette,
        kAimZoom,
        kAimWith,
        kRenderResolution,
        kWorldScale,
        kHudSize,
        kHudDepth,
        kMenuDistance,
        kMenuSize,
        kRecenterView,
        kTurnFade,
        kWeaponFollowsHand,
        kBodyLean,
        kBodyFacing,
        kSupportHand,
        kFingerPoses,
        kWalkingLegs,
        kBodyTracking,
        kHandTracking,
        kMatchBodyHeight,
        kHudPlacement,
        kEyeScreenshots,

        kFirstId = 8600,
        kLastId  = 8699,
    };

    static constexpr int kCustomPreset = 3;

    struct CategoryInfo
    {
        Category    id;
        const char* label;
    };

    struct Option
    {
        Id                       id;
        Category                 category;
        Kind                     kind;
        const char*              label;
        const char*              description;
        std::vector<std::string> choices;
        std::array<int, 3>       preset{ -1, -1, -1 };  // Low, Medium, High; -1 = not part of the preset
        ModCombo::Ptr            value;
    };

    static std::shared_ptr<VROptions>& Get()
    {
        static std::shared_ptr<VROptions> instance{ std::make_shared<VROptions>() };
        return instance;
    }

    VROptions();

    [[nodiscard]] std::string_view get_name() const override { return "VROptions"; }

    void on_config_load(const utility::Config& cfg, bool set_defaults) override;
    void on_config_save(utility::Config& cfg) override;

    [[nodiscard]] static const std::array<CategoryInfo, 3>& categories();
    [[nodiscard]] const std::vector<Option>&                options() const { return m_options; }
    [[nodiscard]] int                                       get(std::uint32_t id) const;
    void                                                    set(std::uint32_t id, int value);

private:
    [[nodiscard]] const Option* find(std::uint32_t id) const;
    [[nodiscard]] int           current_preset() const;
    void                        apply_preset(int preset);
    void                        apply() const;

    std::vector<Option> m_options;
};
