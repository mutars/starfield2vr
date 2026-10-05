#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <d3d12.h>
#include <memory>
#include <memory/FunctionHook.h>
#include <vector>
#include <wrl/client.h>

namespace RE
{
    class NiCamera;
}

// Native stereo: each eye is a camera view of its own, rendered over the whole frame in its own render graph in the same
// engine frame. Each eye's finished image is copied out after the post chain and presented to its eye of the headset.
class StereoViewModule
{
public:
    static StereoViewModule* Get()
    {
        static auto instance(new StereoViewModule);
        return instance;
    }

    void InstallHooks();

    // Game thread, once per engine frame before the cameras are posed.
    void OnFrameStart();

    [[nodiscard]] RE::NiCamera* RightCamera() const { return m_right_camera; }
    // Both eyes render through cloned cameras; the world camera keeps serving game logic but draws no eye.
    [[nodiscard]] RE::NiCamera* LeftCamera() const { return m_left_camera; }
    // Eye a camera renders in native stereo: 0 left, 1 right, -1 not an eye camera.
    [[nodiscard]] int EyeOf(const RE::NiCamera* camera) const;

    // Saves the back buffer and the image each eye of the headset receives on the next frame.
    void RequestEyeScreenshots() { m_screenshot_requested.store(true); }

    enum UpscalerPass
    {
        kDLSSInputs,
        kDLSSUpscale,
        kUpscalerPassCount
    };
    template <int Pass>
    static uintptr_t onUpscalerPass(void* pass, void* render_graph_data, void* pass_data)
    {
        return Get()->RunUpscalerPass(Pass, pass, render_graph_data, pass_data);
    }

    // The tonemap pass and the post effects that may follow it, each re-capturing the eye image.
    static constexpr int kLatePassCount = 7;
    template <int Pass>
    static uintptr_t onLatePass(void* pass, void* render_graph_data, void* pass_data)
    {
        return Get()->RunLatePass(Pass, pass, render_graph_data, pass_data);
    }

private:
    StereoViewModule() = default;

    struct ViewIdArray;
    struct MirroredRows
    {
        bool     valid{ false };
        uint8_t  camera_view_data[0x1C]{};
        uint32_t image_space{ 0 };
        uint8_t  feature_setup[0x210]{};
        uint8_t  directional_shadow[0x3C]{};
        uint8_t  highlight[0x14]{};
    };

    bool TryRegister();
    bool RegisterRightGraph();
    void PrepareRightGraph();
    void KeepEyeViewportsFull();
    void MirrorMainView();
    void MirrorInto(uint32_t main_view, uint32_t view_id, RE::NiCamera* camera, MirroredRows& mirrored);
    void UpdateEyeFrustums();
    void UpdateMenuFallback(bool stereo);
    void ApplyNativeShadowSettings(bool stereo);
    void DisableFrameGeneration();
    [[nodiscard]] uint32_t SceneOf(void* render_graph_data) const;

    void      InstallUpscalerHooks();
    uintptr_t RunUpscalerPass(int pass_kind, void* pass, void* render_graph_data, void* pass_data);
    void      InstallLatePassHooks();
    uintptr_t RunLatePass(int pass_kind, void* pass, void* render_graph_data, void* pass_data);
    void      CaptureEyeImage(uint32_t eye, int pass_kind, void* render_graph_data, void* pass_data);
    void      CaptureUiLayer(void* render_graph_data, void* pass_data);
    // Eye a scene graph renders (0 left, 1 right), or -1.
    [[nodiscard]] int EyeOfGraph(void* render_graph_data) const;

    static uintptr_t onSetMultiCameraViewData(void* column, uint32_t graph_index, ViewIdArray* views);
    static uintptr_t onSubmitRenderGraph(void* frame_list, void* record);
    static uintptr_t onScaleformComposite(void* pass, void* render_graph_data, void* pass_data);

    std::unique_ptr<FunctionHook> m_set_multi_view_hook{};
    std::unique_ptr<FunctionHook> m_submit_graph_hook{};
    std::unique_ptr<FunctionHook> m_scaleform_composite_hook{};
    std::unique_ptr<FunctionHook> m_upscaler_hooks[kUpscalerPassCount]{};
    std::unique_ptr<FunctionHook> m_late_hooks[kLatePassCount]{};

    // The eye cameras and their camera views.
    RE::NiCamera* m_right_camera{ nullptr };
    uint32_t      m_right_view_id{ 0xFFFFFF };
    RE::NiCamera* m_left_camera{ nullptr };
    uint32_t      m_left_view_id{ 0xFFFFFF };
    MirroredRows  m_right_mirrored{};
    MirroredRows  m_left_mirrored{};
    float         m_eye_near{ -1.0f };
    float         m_eye_far{ -1.0f };

    bool m_signatures_ok{ false };
    bool m_registered{ false };
    bool m_failed{ false };
    int  m_register_attempt_frame{ -1000 };

    // The right eye renders in its own render graph, so it never shares the left eye's working buffers.
    alignas(16) uint8_t m_right_graph_record[0x80]{};
    uint32_t            m_right_graph_id{ 0xFFFFFF };
    uint8_t             m_right_graph_options[0x30]{};
    uint8_t             m_right_graph_frame_flag{ 0xFF };
    int                 m_missed_appends{ 0 };
    std::atomic<bool>   m_appended{ false };

    // Fullscreen menus run the game's own single view; they are shown on the flat screen.
    std::atomic<bool> m_menu_fallback{ false };

    // Per eye, one capture texture for each output size the post chain writes.
    std::array<D3D12_RESOURCE_DESC, 2>                                 m_eye_output_desc{};
    std::array<std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>, 2> m_eye_capture{};
    Microsoft::WRL::ComPtr<ID3D12Resource>                             m_ui_capture{};

    bool  m_shadow_settings_applied{ false };
    float m_saved_shadow_fade_seconds{ 0.75f };
    bool  m_saved_shadow_main_view_lod{ true };
    void* m_frame_generation_setting{ nullptr };
    bool  m_frame_generation_looked_up{ false };

    std::atomic<bool> m_screenshot_requested{ false };
};
