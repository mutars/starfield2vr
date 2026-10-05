include(FetchContent)

# Safetyhook
FetchContent_Declare(
        openxr
        GIT_REPOSITORY "https://github.com/KhronosGroup/OpenXR-SDK.git"
        GIT_TAG "release-1.1.63"
)
message("Fetching openxr")
FetchContent_MakeAvailable(openxr)

