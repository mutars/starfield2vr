#include "ModConfig.h"
#include "Framework.hpp"
#include "dgxiProxy.h"

bool verifyLeftHandedCoordinates() {
    // Create forward vector (in left-handed, +Z is forward)
    glm::vec3 forward(0.0f, 0.0f, 1.0f);

    // Create test rotation 90 degrees around Y axis
    glm::mat4 rotationY = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    // Apply rotation
    glm::vec3 result = glm::vec3(rotationY * glm::vec4(forward, 0.0f));

    // In left-handed: rotating +Z 90° around Y should give +X
    // In right-handed: rotating +Z 90° around Y should give -X
    bool isLeftHanded = glm::abs(result.x - 1.0f) < 0.00001f;

    return isLeftHanded;
}


void InitThread(HINSTANCE hModule) {
    Sleep(5000);
    g_framework = std::make_unique<Framework>(hModule);
    spdlog::info("SFVR build " __DATE__ " " __TIME__);
#ifdef GLM_FORCE_LEFT_HANDED
    spdlog::info("GLM_FORCE_LEFT_HANDED is defined function={} clip={}", verifyLeftHandedCoordinates(), GLM_CONFIG_CLIP_CONTROL);
    assert(verifyLeftHandedCoordinates());
#else
    spdlog::info("GLM_FORCE_LEFT_HANDED is not defined");
#endif

}

void StartVR() {
    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)InitThread, NULL, 0, NULL);
}

BOOL APIENTRY DllMain( HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
//        do {
//            Sleep(250);
//        } while (!IsDebuggerPresent());
//        DisableThreadLibraryCalls(hModule);
#ifndef USE_SFSE_PLUGIN
        // As an SFSE plugin, SFSEPlugin_Load starts VR instead.
        StartVR();
#endif
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
