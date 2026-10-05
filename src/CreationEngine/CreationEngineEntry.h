#pragma once
#include <Mod.hpp>

class CreationEngineEntry : public Mod
{
public:
    inline static std::shared_ptr<CreationEngineEntry>& Get()
    {
        static std::shared_ptr<CreationEngineEntry> instance{ std::make_shared<CreationEngineEntry>() };
        return instance;
    }

    [[nodiscard]] inline std::string_view get_name() const override { return "CreationEngine"; }

    std::optional<std::string> on_initialize() override;
    void                       on_draw_ui() override;
    void                       on_config_load(const utility::Config& cfg, bool ) override;
    void                       on_config_save(utility::Config& cfg) override;

private:
    const ModSlider::Ptr m_head_tracking_multiplier{ ModSlider::create(generate_name("HeadTrackingSensitivity"), 0.5, 2.0, 1.0) };
    const ModToggle::Ptr m_decoupled_pitch{ ModToggle::create(generate_name("DecoupledPitch"), false) };
    const ModToggle::Ptr m_pawn_control_rotation{ ModToggle::create(generate_name("PawnControlRotation"), true) };
    const ModToggle::Ptr m_alternative_joy_layout{ ModToggle::create(generate_name("JoyAlternativeLayout"), false) };

    ValueList m_options{*m_head_tracking_multiplier, *m_alternative_joy_layout, *m_decoupled_pitch, *m_pawn_control_rotation };
};
