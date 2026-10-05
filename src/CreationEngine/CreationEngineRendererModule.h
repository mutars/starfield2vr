#pragma once
#include <RE/C/CreationRendererPrivate.h>
#include <_deps/directxtk12-src/Src/PlatformHelpers.h>
#include <_deps/directxtk12-src/Src/d3dx12.h>
#include <safetyhook/inline_hook.hpp>
#include <memory/FunctionHook.h>
#include <windef.h>

#include "math/Math.hpp"
using Microsoft::WRL::ComPtr;

namespace RE
{
    struct WindowRect
    {
        int x;
        int y;
        int cx;
        int cy;
    };

    struct HWindowHolder
    {
        int     windowX;
        int     windowY;
        int     windowCX;
        int     windowCY;
        uint8_t pad[8];
        LPCSTR  windowName;
        HWND    parentWindowHandle;
        uint8_t pad20[128];
        HWND    windowHandle;
        uint8_t trailing[120];
    };

    struct DisplayGameSettings
    {
        WindowRect windowRect;
        WindowRect displayRect;
        uint32_t   flags;
    };

    static_assert(offsetof(HWindowHolder, windowHandle) == 168);
    static_assert(sizeof(HWindowHolder) == 0x128);

    struct CreationEngineSettings
    {
        uint64_t            unk0;
        uint64_t            unk1;
        DisplayGameSettings displayGameSettings;
        uint8_t             pad2[2];
        int                 windowHeight;
        int                 windowWidth;
        int                 textureHeight;
        int                 textureWidth;
        HWindowHolder*      pHwindow;
        uint8_t             pad68[24];
        uint64_t            features;
        uint8_t             trainling[1024];
    };
    static_assert(offsetof(CreationEngineSettings, pHwindow) == 0x48);
    static_assert(offsetof(CreationEngineSettings, displayGameSettings) == 0x10);

    struct DlssConstants
    {
        uint8_t    pad[0x50];
        Matrix4x4f projectionMatrix;
        uint8_t    pad1[0xC0];
        Matrix4x4f previousProjectionMatrix;
    };

    static_assert(offsetof(DlssConstants, projectionMatrix) == 0x50);
    static_assert(offsetof(DlssConstants, previousProjectionMatrix) == 0x150);

    // aka CameraBlocks
    struct RenderPassConstantBufferView
    {
        Vector2f   unk0;
        int        unk8;
        char       pad_000C[68];
        Matrix4x4f unk50;
        char       pad_0090[64];
        Matrix4x4f unk0D0;
        Vector4f   unk110;
        Vector4f   unk120;
        Vector4f   unk130;
        char       pad_0140[16];
        Matrix4x4f unk150;
        Vector2f   unk190;
        int        unk198;
        char       pad_019C[4];
        Vector4f   unk1A0;
        Vector4f   unk1B0;
        Vector4f   unk1C0;
        Vector4f   unk1D0;
        Matrix4x4f unk1E0;
        char       pad220[68];
        float      horizontalHalfFov;
        float      verticalHalfFov;
        float      unk26C;
        float      cameraNear;
        float      cameraFar;
        char       trail[56];
    };

    static_assert(sizeof(RenderPassConstantBufferView) == 0x2B0);
    static_assert(offsetof(RenderPassConstantBufferView, unk50) == 0x50);
    static_assert(offsetof(RenderPassConstantBufferView, unk0D0) == 0xD0);

    struct MotionVectorRenderPassConstants
    {
        Matrix4x4f currentProjectionMatrix;
        Matrix4x4f previousProjectionMatrix;
        Vector4f   currentCameraPosition;
        Vector4f   previousCameraPosition;
    };

    struct D3D12Context;
    struct D3DContextCommnadAllocatorData;
    struct D3DContextBindData;
#pragma pack(push, 1)

    struct RenderGraphDataD3D12Context
    {
        char pad[0x60];
        ID3D12GraphicsCommandList*      pID3D12CommandList;
        D3DContextBindData*             pD3DContextBindData;
        D3DContextBindData*             pD3DContextBindDataDefault;
        uint8_t                         gap_28[16];
        uint64_t                        field_38;
        uint8_t                         raw_data[4096];
    };

#pragma pack(pop)
    static_assert(offsetof(RenderGraphDataD3D12Context, pID3D12CommandList) == 0x60);
} // namespace RE

class CreationEngineRendererModule
{
public:
    void InstallHooks();

    inline static CreationEngineRendererModule* Get()
    {
        static auto instance(new CreationEngineRendererModule);
        return instance;
    }

    [[nodiscard]] inline RE::CreationEngineSettings* GetCreationEngineSettings() const { return *m_creationEngineSettings; }

    __int64   onRenderGraphRenderStart(RE::CreationRendererPrivate::RenderGraph* pGraph, RE::CreationRendererPrivate::RenderGraphData* pRenderGraphData, __int64 i1, __int64 i2);
//    __int64   onRenderFrameStart(void* pVoid, __int64 i, __int64 i1, __int64 i2);
    void      SetWindowSize(int width, int height);

private:
    CreationEngineRendererModule()  = default;
    ~CreationEngineRendererModule() = default;

    std::unique_ptr<FunctionHook> m_onRenderGraphRenderStartHook{};
    std::unique_ptr<FunctionHook> m_onRenderFrameStartHook{};
    safetyhook::InlineHook m_worldTick_hook{};
    safetyhook::InlineHook m_setReflexMarkerInternalHook{};


    RE::CreationRendererPrivate::RenderPass* m_startFramePass{ nullptr };

    RE::CreationEngineSettings**          m_creationEngineSettings{ nullptr };
    std::mutex                            m_last_resolution_mutex{};
    std::chrono::steady_clock::time_point m_last_resolution_sync{};

    void             RenderGraphStart(RE::CreationRendererPrivate::RenderGraph* pGraph, RE::CreationRendererPrivate::RenderGraphData* pRenderGraphData, bool before);
    static uintptr_t setReflexMarkerInternal(uintptr_t rcx, uint32_t marker, uint32_t oldFrameIndex);
};
