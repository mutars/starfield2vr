#include "GameSettingsComponent.h"
#include "StereoViewModule.h"
#include "CreationEngineRendererModule.h"
#include "CreationEngineSettings.h"
#include <mods/VR.hpp>

std::optional<std::string> GameSettingsComponent::on_initialize()
{
    spdlog::info("GameSettingsComponent::on_initialize");
    return Mod::on_initialize();
}

std::optional<std::string> GameSettingsComponent::on_initialize_d3d_thread()
{
    spdlog::info("GameSettingsComponent::on_initialize_d3d_thread");
    return Mod::on_initialize_d3d_thread();
}

void GameSettingsComponent::on_frame()
{

}


void GameSettingsComponent::on_device_reset()
{
    spdlog::info("GameSettingsComponent::on_device_reset");
    auto vr = VR::get();
    if(vr->get_runtime()->loaded) {
        static auto creation_engine_settings = CreationEngineSettings::Get();
        float aspect = (float)vr->get_hmd_width() / (float)vr->get_hmd_height();
        //        spdlog::info("VR Runtime is loaded {}x{} fov={}", vr->get_hmd_width(), vr->get_hmd_height(), vr->get_runtime()->diagonal_fov);
        creation_engine_settings->set_setting("fWideAspectLimit:Display", CreationEngineSettings::SettingType::kINISetting, aspect);
        creation_engine_settings->set_setting("fNarrowAspectLimit:Display", CreationEngineSettings::SettingType::kINISetting, aspect);
        creation_engine_settings->set_setting("fFPWorldFOV:Camera", CreationEngineSettings::SettingType::kINIPrefSetting, vr->get_runtime()->diagonal_fov);
        creation_engine_settings->set_setting("fTPWorldFOV:Camera", CreationEngineSettings::SettingType::kINIPrefSetting, vr->get_runtime()->diagonal_fov);
        creation_engine_settings->set_setting("fTPShipFOV:Camera", CreationEngineSettings::SettingType::kINIPrefSetting, vr->get_runtime()->diagonal_fov);
        if(!m_override_weapon_fov->value()) {
            creation_engine_settings->set_setting("fFPGeometryFOV:Camera", CreationEngineSettings::SettingType::kINISetting, vr->get_runtime()->diagonal_fov);
        }
        // Depth of field blurs whatever is close to the eyes in a headset.
        const bool dof_off = creation_engine_settings->set_setting("bDepthOfFieldEnable:Display", CreationEngineSettings::SettingType::kINIPrefSetting, false) ||
                             creation_engine_settings->set_setting("bDepthOfFieldEnable:Display", CreationEngineSettings::SettingType::kINISetting, false);
        spdlog::info("Depth of field {}", dof_off ? "disabled" : "setting not found");
    };
}

void GameSettingsComponent::on_draw_ui()
{
    if (!ImGui::CollapsingHeader(get_name().data())) {
        return;
    }
    ImGui::Separator();
    auto creation_engine_settings = CreationEngineSettings::Get();
    {
        m_override_weapon_fov->draw("Override Weapon Fov");
        if (m_override_weapon_fov->value()) {
            ImGui::SameLine();
            if(m_weapon_fov->draw("Weapon Fov")) {
                creation_engine_settings->set_setting("fFPGeometryFOV:Camera", CreationEngineSettings::SettingType::kINISetting, m_weapon_fov->value());
            }
        }
    }

#ifdef _DEBUG
    {
        //"fNarrowAspectLimit:Display"
        static float wideAspectLimit = creation_engine_settings->get_setting("fWideAspectLimit:Display", CreationEngineSettings::SettingType::kINISetting, 1.5f);
        if (ImGui::SliderFloat("fWideAspectLimit:Display", &wideAspectLimit, 0.8f, 1.5f)) {
            creation_engine_settings->set_setting("fWideAspectLimit:Display", CreationEngineSettings::SettingType::kINISetting, wideAspectLimit);
        }
    }
    {
        static int iSizeW = creation_engine_settings->get_setting("iSize W:Display", CreationEngineSettings::SettingType::kINIPrefSetting, 1920);
        if(ImGui::InputInt("iSize W:Display", &iSizeW)) {
            creation_engine_settings->set_setting("iSize W:Display", CreationEngineSettings::SettingType::kINIPrefSetting, iSizeW);
        }
    }
    {
        static bool scan = false;
        if(ImGui::Checkbox("Scan Options", &scan)) {
            if(scan) {
                CreationEngineSettings::get_setting("test", CreationEngineSettings::SettingType::kGameSetting);
                CreationEngineSettings::get_setting("test", CreationEngineSettings::SettingType::kRendererQualitySetting);
                CreationEngineSettings::get_setting("test", CreationEngineSettings::SettingType::kRendererPerfSetting);
                CreationEngineSettings::get_setting("test", CreationEngineSettings::SettingType::kINISetting);
                CreationEngineSettings::get_setting("test", CreationEngineSettings::SettingType::kINIPrefSetting);
            }
        }
    }
#endif
}

void GameSettingsComponent::on_config_load(const utility::Config& cfg, bool set_default) {
    for (IModValue& option : m_options) {
        option.config_load(cfg, set_default);
    }
    if(m_override_weapon_fov->value()) {
        CreationEngineSettings::Get()->set_setting("fFPGeometryFOV:Camera", CreationEngineSettings::SettingType::kINISetting, m_weapon_fov->value());
    }
}

void GameSettingsComponent::on_config_save(utility::Config& cfg)
{
    for (IModValue& option : m_options) {
        option.config_save(cfg);
    }
}
