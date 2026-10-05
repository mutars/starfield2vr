#include "CreationEngineEntry.h"
#include "CreationEngineConstants.h"
#include "imgui.h"
#include <CreationEngine/models/GameFlow.h>
#include <CreationEngine/models/ModSettingsStore.h>
#include <imgui_internal.h>
#include <mods/VR.hpp>
#include <vector>

#include "CreationEngineCameraManager.h"
#include "CreationEngineInputManager.h"
#include "CreationEngineRendererModule.h"
#include "MuzzleFire.h"
#include "StereoViewModule.h"

std::optional<std::string> CreationEngineEntry::on_initialize()
{
    CreationEngineCameraManager::Get()->InstallHooks();
    CreationEngineRendererModule::Get()->InstallHooks();
    StereoViewModule::Get()->InstallHooks();
    CreationEngineInputManager::Get()->Init();
    MuzzleFire::Install();
    return Mod::on_initialize();
}

void CreationEngineEntry::on_draw_ui()
{
    if (!ImGui::CollapsingHeader(get_name().data())) {
        return;
    }
    auto vr = VR::get();
    if(GameFlow::gStore.internalSettings.controllerLayout == 1 && m_alternative_joy_layout->draw("Alternative Joy Layout"))
    {
        GameFlow::gStore.internalSettings.alternativeJoyLayout = m_alternative_joy_layout->value();
    }
    if(m_pawn_control_rotation->draw("Pawn Control Rotation"))
    {
        GameFlow::gStore.internalSettings.pawnControl = m_pawn_control_rotation->value();
    }
    if(m_decoupled_pitch->draw("Decoupled Pitch"))
    {
        GameFlow::gStore.internalSettings.decoupledPitch = m_decoupled_pitch->value();
    }
    if(m_head_tracking_multiplier->draw("Head Tracking Sensitivity"))
    {
        ModConstants::headTrackingMultiplier = m_head_tracking_multiplier->value();
    }

    for(auto& ui_part : GameFlow::gStore.debugData.ui_parts)
    {
        ImGui::Text("UI: %s", ui_part.data());
    }
#if 0
    if(0){
        auto player = CreationEngineSingletonManager::GetPlayerRef();
        GameFlow::State::DebugWeaponData& debugWeaponData = GameFlow::gState.debugWeaponData;

        if(player){
            ImGui::Checkbox("Debug Weapon [X] - LaserNode [ ] - Projectile", &debugWeaponData.type);
            ImGui::InputInt("Deep Level", &debugWeaponData.deepLevel, 0, 10);

            RE::NiNode* obj_int{};
            if(debugWeaponData.type) {
                obj_int = player->getLaserNode(debugWeaponData.deepLevel);
            } else {
                obj_int = player->getProjectileNode(debugWeaponData.deepLevel);
            }

            ImGui::InputFloat3("yaw,roll,pitch", &debugWeaponData.yaw);

            if(obj_int)
            {
                ImGui::Text("Current Node [%s] %s", debugWeaponData.type? "Laser": "Projectile" ,obj_int->name.data());
                auto local = glm::mat4(*(glm::mat3x4*) &obj_int->local.rotate);
                local = glm::rowMajor4(local);
                auto euler = glm::eulerAngles(glm::quat{local});
                ImGui::Text("Current Euler [%.2f,%.2f,%.2f]", euler.x, euler.y, euler.z);
                auto qua = glm::quat{local};
                ImGui::Text("Current quaternion [x=%.2f,y=%.2f,z=%.2f,w=%.2f]", qua.x, qua.y, qua.z, qua.w);
            }

            {
                RE::NiNode* obj = player->getLaserNode(0);
                std::string laser_hierarchy = "$";
                while(obj)
                {
                    laser_hierarchy = obj->name.data() + std::string(":") + laser_hierarchy;
                    obj = obj->parent;
                }
                obj = player->getProjectileNode(0);
                std::string projectile_hierarchy = "$";
                while(obj)
                {
                    projectile_hierarchy = obj->name.data() + std::string(":") + projectile_hierarchy;
                    obj = obj->parent;
                }
                ImGui::Text("[Laser] %s", laser_hierarchy.data());
                ImGui::Text("[Projectile] %s", projectile_hierarchy.data());
            }
        }
    }


    {
        static auto camera = CreationEngineSingletonManager::GetSceneGraphRoot()->worldCamera;
        auto rt_w    = (float)g_framework->get_rendertarget_width_d3d12();
        auto rt_h    = (float)g_framework->get_rendertarget_height_d3d12();
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(rt_w, rt_h));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::Begin("Circle", nullptr,
                     ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMouseInputs | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground);

        auto* draw_list = ImGui::GetWindowDrawList();
        int index = 0;
        std::vector<ImRect> drawnRects;

        {

            for (auto& point : GameFlow::gStore.debugData.points) {

                ImVec2 center{};
                auto wts = camera->WorldToScreenNormalized({ point.x, point.y, point.z });

                // Compute center position in render target space
                center.x = wts.x * rt_w;
                center.y = wts.y * rt_h;

                // Draw the point
                draw_list->AddCircleFilled(center, 5.0f, IM_COL32(255, 0, 0, 255));

                // Create the label string for this point
                char label[64];
                sprintf_s(label, "%d:[%.1f,%.1f,%.1f]", (int)(index / 2), point.x, point.y, point.z);

                // Calculate text size
                ImVec2 labelSize = ImGui::CalcTextSize(label);
                // Start with a zero offset in Y
                float offsetY = 0.0f;
                ImRect currRect(center.x, center.y + 5 + offsetY, center.x + labelSize.x, center.y + 5 + offsetY + labelSize.y);

                // Set a maximum offset value so that it eventually exits if offsets go too high
                const float maxOffset = rt_h;

                while (true) {
                    bool hasOverlap = false;
                    for (const auto& r : drawnRects) {
                        if (currRect.Overlaps(r)) {
                            hasOverlap = true;
                            break;
                        }
                    }
                    if (!hasOverlap || offsetY >= maxOffset) {
                        break;
                    }
                    offsetY += labelSize.y;
                    currRect = ImRect(center.x, center.y + 5 + offsetY, center.x + labelSize.x, center.y + 5 + offsetY + labelSize.y);
                }
                
                // Draw the label and save its rectangle for future overlap checks
                draw_list->AddText(ImVec2(center.x, center.y + 5 + offsetY), IM_COL32(255, 255, 255, 255), label);
                drawnRects.push_back(currRect);

                index++;
            }
        }
        ImGui::End();
        ImGui::PopStyleVar();
        GameFlow::gStore.debugData.points.clear();
    }
#endif
}

void CreationEngineEntry::on_config_load(const utility::Config& cfg, bool set_defaults) {
    for (IModValue& option : m_options) {
        option.config_load(cfg, set_defaults);
    }
    ModConstants::headTrackingMultiplier = m_head_tracking_multiplier->value();
    GameFlow::gStore.internalSettings.pawnControl = m_pawn_control_rotation->value();
    GameFlow::gStore.internalSettings.alternativeJoyLayout = m_alternative_joy_layout->value();
    GameFlow::gStore.internalSettings.decoupledPitch = m_decoupled_pitch->value();
}

void CreationEngineEntry::on_config_save(utility::Config& cfg)
{
    for (IModValue& option : m_options) {
        option.config_save(cfg);
    }
}
