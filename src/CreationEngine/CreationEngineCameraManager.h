#pragma once
#include <RE/N/NiCamera.h>
#include <RE/N/NiMatrix3.h>
#include <glm/vec3.hpp>
#include <memory/FunctionHook.h>
#include <RE/F/FirstPersonState.h>
#include <RE/N/NiQuaternion.h>

#include "math/Math.hpp"

struct Matrix6x4f
{
    float m[6][4];
};

namespace RE
{
    struct NiUpdateData;
}

namespace CreationEngine
{
    struct Viewport
    {
        float left;
        float right;
        float bottom;
        float top;
    };
} // namespace CreationEngine

namespace Scaleform::Gfx
{
    struct alignas(4) Viewport
    {
    public:
        enum class Flag : std::uint32_t
        {
            kIsRenderTexture = 1 << 0,

            kAlphaComposite = 1 << 1,
            kUseScissorRect = 1 << 2,

            kNoSetState = 1 << 3,

            kOrientation_Normal = 0,
            kOrientation_R90    = 1 << 4,
            kOrientation_180    = 1 << 5,
            kOrientation_L90    = kOrientation_R90 | kOrientation_180,
            kOrientation_Mask   = kOrientation_Normal | kOrientation_R90 | kOrientation_180 | kOrientation_L90,

            kStereo_SplitV   = 1 << 6,
            kStereo_SplitH   = 1 << 7,
            kStereo_AnySplit = kStereo_SplitV | kStereo_SplitH,

            kRenderTextureAlpha = kIsRenderTexture | kAlphaComposite,

            kFirstHalFlag = 1 << 8
        };
        std::int32_t bufferWidth;         // 00
        std::int32_t bufferHeight;        // 04
        std::int32_t left;                // 08
        std::int32_t top;                 // 0C
        std::int32_t width;               // 10
        std::int32_t height;              // 14
        std::int32_t scissorLeft;         // 18
        std::int32_t scissorTop;          // 1C
        std::int32_t scissorWidth;        // 20
        std::int32_t scissorHeight;       // 24
                                          //        stl::enumeration<Flag, std::uint32_t> flags;              // 28
        std::uint32_t flags;              // 28
        float         scale       = 1.0f; // 0x2C
        float         aspectRatio = 1.0f; // 0x30
    };

    static_assert(offsetof(Viewport, scale) == 0x2C);
    static_assert(sizeof(Viewport) == 0x34);
} // namespace Scaleform::Gfx

class CreationEngineCameraManager
{
public:
    void InstallHooks();

    inline static CreationEngineCameraManager* Get()
    {
        static auto instance(new CreationEngineCameraManager);
        return instance;
    }

    void                          UpdateWorldCamera();
    static void                   SnapshotAimPose();
    // Muzzle of the weapon held by the shown body, or null when the body is not driving the weapon.
    static RE::NiAVObject*        GetBodyMuzzle();
    // Aim direction (game world) the held weapon was posed along this frame, if any.
    static bool                   GetBodyAimForward(float out[3]);
    // The player fired: the weapon's pose in the first-person rig is sampled as its firing grip.
    static void                   NotifyPlayerFired();
    static void onNiAVObjectUpdateWorld(RE::NiAVObject* obj, RE::NiUpdateData* a_data);
    void onScaleformSetViewPort(uintptr_t* thisMovie, Scaleform::Gfx::Viewport* viewport);

    CreationEngineCameraManager(const CreationEngineCameraManager&)            = delete;
    CreationEngineCameraManager& operator=(const CreationEngineCameraManager&) = delete;

    void onSetNimFrustum(RE::NiCamera* pCamera, RE::NiFrustum* pFrustum);

    void onScaleformMovieSetProjectionMatrix3D(uintptr_t* pInt, Matrix4x4f& mat);
    void onCalcNiFrustum(RE::NiCamera* pCamera, float fov, float aspectRatio, float nearz, float farz, char lodAdjust);

    [[nodiscard]] float get_fov_adjustment() const;

private:
    CreationEngineCameraManager()  = default;
    ~CreationEngineCameraManager() = default;
    std::unique_ptr<FunctionHook> m_onNiAVObjectUpdateWorldHook{};
    std::unique_ptr<FunctionHook> m_onUpdateWorldHook{};
    std::unique_ptr<FunctionHook> m_onPerformInputProcessingHook{};
    std::unique_ptr<FunctionHook> m_onCameraCutProcessEventHook{};
    std::unique_ptr<FunctionHook> m_onSetNimFrustumHook{};
    std::unique_ptr<FunctionHook> m_onCalcNimFrustumHook{};
    std::unique_ptr<FunctionHook> m_onScaleformSetViewPortHook{};
    std::unique_ptr<FunctionHook> m_onScaleformMovieSetProjectionMatrix3DHook{};
    std::unique_ptr<FunctionHook> m_onUpdateWorldToCamHook{};
    std::unique_ptr<FunctionHook> m_onUpdateCollisionMatrixHook{};
    std::unique_ptr<FunctionHook> m_onSetViewportHook{};
    std::unique_ptr<FunctionHook> m_onSetCameraScissorHook{};
    std::unique_ptr<FunctionHook> m_onGetCameraRotationHook{};
    std::unique_ptr<FunctionHook> m_onFirstPersonStateUpdatePitchHook{};
//    std::unique_ptr<FunctionHook> m_onGetCameraLocationHook{};
//    void                          UpdateCamera(RE::NiCamera* a_camera, RE::NiUpdateData* a_data);
//    void                          rotateCamera(RE::NiCamera* a_camera, const RE::NiMatrix3* a_rot);
    void                          onSetNiFrustumInternal(RE::NiCamera* pCamera, RE::NiFrustum* pFrustum);
    void                          onScaleformSetViewPortInternal(uintptr_t* thisMovie, Scaleform::Gfx::Viewport* viewport);
    static void onFPSGetCameraRotation(RE::FirstPersonState* fps, RE::NiQuaternion* quat_out);
    unsigned int*                 m_globalFrameCount{};
    float*                        m_globalWorldFov{};

    //    glm::vec3                                    prev_euler{ 0.0f, 0.0f, 0.0f };
    glm::quat                                    prev_quat{ 1.0f, 0.0f, 0.0f, 0.0f };
    std::unordered_map<uintptr_t, RE::NiMatrix3> originalRotations{};
    float                                        m_fov_adjust{ 0.0f };
    [[nodiscard]] float                          get_head_tracking_multiplier() const;
};
