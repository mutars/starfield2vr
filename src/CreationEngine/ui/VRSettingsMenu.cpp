#include "VRSettingsMenu.h"

#include <atomic>
#include <mutex>
#include <unordered_map>

#include "GFx.h"
#include <CreationEngine/VROptions.h>

namespace VRSettingsMenu
{
    namespace
    {
        // SettingsOptionListEntry.SDT_*
        constexpr std::uint32_t kTypeStepper  = 1;
        constexpr std::uint32_t kTypeCheckBox = 3;
        constexpr std::uint32_t kTypeLink     = 4;

        constexpr int kTickInterval = 5;

        bool IsOurRow(std::uint32_t id) { return id >= VROptions::kFirstId && id <= VROptions::kLastId; }

        std::atomic<bool> g_rows_dirty{ false };

        class ValueChangeHandler final : public GFx::FunctionHandler
        {
        public:
            void Call(const GFx::FunctionParams* params) override
            {
                if (!params || params->argCount < 1) {
                    return;
                }
                auto& event = params->args[0];

                GFx::Value payload, id, value;
                if (!event.GetMember("params", &payload) || !payload.GetMember("id", &id) || !payload.GetMember("value", &value)) {
                    return;
                }
                const auto setting_id = static_cast<std::uint32_t>(id.GetNumber());
                if (!IsOurRow(setting_id)) {
                    return;
                }

                // Our IDs must not reach the game's settings code.
                event.Invoke("stopPropagation", nullptr, nullptr, 0);

                const auto new_value = static_cast<int>(value.GetNumber());
                spdlog::info("[VRSettingsMenu] Setting {} changed to {}", setting_id, new_value);
                VROptions::Get()->set(setting_id, new_value);
                g_rows_dirty = true;  // the preset row and linked rows need redrawing
            }
        };

        ValueChangeHandler g_value_change_handler;
        void*              g_listened_list{ nullptr };
        int                g_frame{ 0 };

        std::mutex                       g_menus_mutex;
        std::unordered_map<void*, void*> g_menu_by_movie;  // movie -> IMenu

        void SetText(GFx::Value& obj, const char* name, const char* text) { obj.SetMember(name, GFx::Value(text)); }

        std::uint32_t RowType(VROptions::Kind kind)
        {
            switch (kind) {
            case VROptions::Kind::Toggle:
                return kTypeCheckBox;
            case VROptions::Kind::Action:
                return kTypeLink;
            default:
                return kTypeStepper;
            }
        }

        void MakeRow(GFx::MovieRoot* root, GFx::Value* out, const VROptions::Option& option)
        {
            root->CreateObject(out);
            out->SetMember("uID", GFx::Value(static_cast<std::uint32_t>(option.id)));
            out->SetMember("uCategory", GFx::Value(static_cast<std::uint32_t>(option.category)));
            out->SetMember("uType", GFx::Value(RowType(option.kind)));
            SetText(*out, "sText", option.label);
            SetText(*out, "sDescription", option.description);
            SetText(*out, "sPreview", "");
            out->SetMember("bEnabled", GFx::Value(true));
            out->SetMember("bSubSetting", GFx::Value(false));

            // SettingsOptionListEntry.SetEntryText reads every block regardless of type.
            GFx::Value slider;
            root->CreateObject(&slider);
            slider.SetMember("fValue", GFx::Value(0.0));
            SetText(slider, "sDisplayValue", "");
            out->SetMember("sliderData", slider);

            GFx::Value stepper, choices;
            root->CreateObject(&stepper);
            root->CreateArray(&choices);
            if (option.kind == VROptions::Kind::Choice || option.kind == VROptions::Kind::Preset) {
                for (auto& choice : option.choices) {
                    choices.PushBack(GFx::Value(choice.c_str()));
                }
            }
            stepper.SetMember("aStepperOptions", choices);
            stepper.SetMember("uIndex", GFx::Value(0u));
            out->SetMember("stepperData", stepper);

            GFx::Value checkbox;
            root->CreateObject(&checkbox);
            checkbox.SetMember("bChecked", GFx::Value(false));
            out->SetMember("checkBoxData", checkbox);
        }

        void RefreshRowValue(GFx::Value& row, std::uint32_t id)
        {
            const auto options = VROptions::Get();
            const auto value   = options->get(id);
            GFx::Value block;
            if (row.GetMember("checkBoxData", &block)) {
                block.SetMember("bChecked", GFx::Value(value != 0));
            }
            if (row.GetMember("stepperData", &block)) {
                block.SetMember("uIndex", GFx::Value(static_cast<std::uint32_t>(value)));
            }
        }

        bool ArrayHasId(GFx::Value& array, std::uint32_t wanted, bool refreshOurs)
        {
            bool       found = false;
            const auto size  = array.GetArraySize();
            for (std::uint32_t i = 0; i < size; ++i) {
                GFx::Value element, uid;
                if (!array.GetElement(i, &element) || !element.GetMember("uID", &uid)) {
                    continue;
                }
                const auto id = static_cast<std::uint32_t>(uid.GetNumber());
                if (id == wanted) {
                    found = true;
                }
                if (refreshOurs && IsOurRow(id)) {
                    RefreshRowValue(element, id);
                }
            }
            return found;
        }

        bool GetProvider(GFx::Value& dataManager, const char* name, GFx::Value* provider, GFx::Value* data)
        {
            // Don't create the provider before the game does.
            GFx::Value args[] = { GFx::Value(name), GFx::Value(false) };
            if (!dataManager.Invoke("GetDataFromClient", provider, args, 2) || !provider->IsObjectLike()) {
                return false;
            }
            return provider->GetMember("data", data) && data->IsObjectLike();
        }

        void EnsureCategory(GFx::MovieRoot* root, GFx::Value& dataManager)
        {
            GFx::Value provider, data, headers;
            if (!GetProvider(dataManager, "SettingsCategoriesData", &provider, &data) || !data.GetMember("aCategoryHeaders", &headers)) {
                return;
            }
            if (headers.GetArraySize() == 0) {
                return;
            }

            bool added = false;
            for (auto& category : VROptions::categories()) {
                if (ArrayHasId(headers, category.id, false)) {
                    continue;
                }
                GFx::Value header;
                root->CreateObject(&header);
                header.SetMember("uID", GFx::Value(static_cast<std::uint32_t>(category.id)));
                SetText(header, "sText", category.label);
                header.SetMember("bDisabled", GFx::Value(false));
                headers.PushBack(header);
                added = true;
            }

            if (added) {
                provider.Invoke("DispatchChange", nullptr, nullptr, 0);
                spdlog::info("[VRSettingsMenu] Added VR categories");
            }
        }

        void EnsureRows(GFx::MovieRoot* root, GFx::Value& dataManager)
        {
            GFx::Value provider, data, rows;
            if (!GetProvider(dataManager, "SettingsData", &provider, &data) || !data.GetMember("aGeneralSettingsList", &rows)) {
                return;
            }

            const bool present = ArrayHasId(rows, VROptions::kComfortPreset, true);
            if (present) {
                if (g_rows_dirty.exchange(false)) {
                    provider.Invoke("DispatchChange", nullptr, nullptr, 0);
                }
                return;
            }

            for (auto& option : VROptions::Get()->options()) {
                GFx::Value row;
                MakeRow(root, &row, option);
                RefreshRowValue(row, option.id);
                rows.PushBack(row);
            }
            g_rows_dirty = false;
            provider.Invoke("DispatchChange", nullptr, nullptr, 0);
            spdlog::info("[VRSettingsMenu] Added VR rows");
        }

        void EnsureListener(GFx::MovieRoot* root, GFx::Value& panel)
        {
            GFx::Value options_panel, list;
            if (!panel.GetMember("OptionsPanel_mc", &options_panel) || !options_panel.GetMember("OptionsList_mc", &list)) {
                return;
            }
            if (list.GetData() == g_listened_list) {
                return;
            }

            GFx::Value args[] = { GFx::Value("SettingsOptionEntry_ValueChanged"), GFx::Value() };
            root->CreateFunction(&args[1], &g_value_change_handler);
            if (list.Invoke("addEventListener", nullptr, args, 2)) {
                g_listened_list = list.GetData();
                spdlog::info("[VRSettingsMenu] Listening for option changes");
            }
        }
    }

    void OnMenuMovieCreated(void* menu)
    {
        if (!menu) {
            return;
        }
        // IMenu: GetName() is vfunc 3, the movie is at +0x88 and the root object (menuObj) at +0x58.
        auto       vtable = *reinterpret_cast<void***>(menu);
        const auto name   = reinterpret_cast<const char* (*)(void*)>(vtable[3])(menu);
        auto       movie  = *reinterpret_cast<void**>(static_cast<std::uint8_t*>(menu) + 0x88);
        if (!name || !movie || (std::string_view{ name } != "PauseMenu" && std::string_view{ name } != "MainMenu")) {
            return;
        }
        std::scoped_lock _{ g_menus_mutex };
        g_menu_by_movie[movie] = menu;
        spdlog::info("[VRSettingsMenu] Tracking {} movie {:p}", name, movie);
    }

    void OnMovieFrame(void* movieImpl, std::string_view fileUrl)
    {
        if (fileUrl != "Interface/PauseMenu.swf" && fileUrl != "Interface/PauseMenu_LRG.swf" &&
            fileUrl != "Interface/MainMenu.swf" && fileUrl != "Interface/MainMenu_LRG.swf") {
            return;
        }
        if (++g_frame % kTickInterval != 0) {
            return;
        }

        auto root = GFx::MovieRoot::FromMovie(movieImpl);
        if (!root) {
            return;
        }

        const GFx::Value* stage_root = nullptr;
        {
            std::scoped_lock _{ g_menus_mutex };
            if (auto it = g_menu_by_movie.find(movieImpl); it != g_menu_by_movie.end()) {
                stage_root = reinterpret_cast<const GFx::Value*>(static_cast<std::uint8_t*>(it->second) + 0x58);
            }
        }
        if (!stage_root || !stage_root->IsObjectLike()) {
            return;
        }

        GFx::Value panel, visible;
        if (!stage_root->GetMember("SettingsPanel_mc", &panel) || !panel.GetMember("visible", &visible) || !visible.GetBool()) {
            return;
        }

        GFx::Value loader_info, domain, data_manager;
        GFx::Value class_name[] = { GFx::Value("Shared.AS3.Data.BSUIDataManager") };
        if (!stage_root->GetMember("loaderInfo", &loader_info) || !loader_info.GetMember("applicationDomain", &domain) ||
            !domain.Invoke("getDefinition", &data_manager, class_name, 1) || !data_manager.IsObjectLike()) {
            static bool logged = false;
            if (!logged) {
                logged = true;
                spdlog::warn("[VRSettingsMenu] BSUIDataManager class not reachable");
            }
            return;
        }

        EnsureListener(root, panel);
        EnsureCategory(root, data_manager);
        EnsureRows(root, data_manager);
    }
}
