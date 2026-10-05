#pragma once
#include <string_view>

// Adds a VR category to the game's Settings menu by extending its BSUIDataManager payloads.
namespace VRSettingsMenu
{
    void OnMenuMovieCreated(void* menu);
    void OnMovieFrame(void* movieImpl, std::string_view fileUrl);
}
