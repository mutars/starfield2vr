#ifdef USE_SFSE_PLUGIN

#include <cstdint>
#include <windows.h>

// From sfse/PluginAPI.h.
namespace
{
    struct SFSEInterface
    {
        std::uint32_t sfseVersion;
        std::uint32_t runtimeVersion;
        std::uint32_t interfaceVersion;
        void* (*QueryInterface)(std::uint32_t id);
    };

    struct SFSEPluginVersionData
    {
        std::uint32_t dataVersion;
        std::uint32_t pluginVersion;
        char          name[256];
        char          author[256];
        std::uint32_t addressIndependence;
        std::uint32_t structureIndependence;
        std::uint32_t compatibleVersions[16];
        std::uint32_t seVersionRequired;
        std::uint32_t reservedNonBreaking;
        std::uint32_t reservedBreaking;
    };

    struct SFSEMenuInterface
    {
        std::uint32_t interfaceVersion;
        void (*RegisterMenuMovieCreated)(void (*callback)(void* menu));
    };

    constexpr std::uint32_t kInterface_Menu = 3;

    constexpr std::uint32_t kAddressIndependence_Signatures         = 1 << 0;
    constexpr std::uint32_t kStructureIndependence_1_14_70_Layout = 1 << 3;
} // namespace

void StartVR();

namespace VRSettingsMenu
{
    void OnMenuMovieCreated(void* menu);
}

extern "C" {
__declspec(dllexport) SFSEPluginVersionData SFSEPlugin_Version = {
    1, // kVersion
    1,
    "SFVR",
    "mutars, AetherianArtificer",
    kAddressIndependence_Signatures,
    kStructureIndependence_1_14_70_Layout,
    { 0 },
    0,
    0,
    0,
};

__declspec(dllexport) bool SFSEPlugin_Load(const SFSEInterface* sfse)
{
    if (sfse && sfse->QueryInterface) {
        if (auto menu = static_cast<SFSEMenuInterface*>(sfse->QueryInterface(kInterface_Menu)); menu && menu->RegisterMenuMovieCreated) {
            menu->RegisterMenuMovieCreated(&VRSettingsMenu::OnMenuMovieCreated);
        }
    }
    StartVR();
    return true;
}
}

#endif
