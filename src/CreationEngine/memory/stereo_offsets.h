#pragma once
#include "ScanHelper.h"

// Engine functions used to register and drive a second camera view (the right eye). Static offsets are for 1.16.244.
namespace Steam::MemoryOffsets::Stereo
{
    using namespace MemoryScan;

    inline uintptr_t NiCameraConstruct()
    {
        static auto addr = FuncRelocation("48 89 4C 24 08 53 48 83 EC 70 48 8B D9 E8", 0x2be2be0);
        return addr;
    }

    inline uintptr_t NiCameraRegisterAsRenderCamera()
    {
        static auto addr = FuncRelocation("48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 20 48 8B F1 48 8D 99 08", 0x2be2e70);
        return addr;
    }

    inline uintptr_t NiCameraSetNearFar()
    {
        static auto addr = FuncRelocation("48 89 5C 24 10 57 48 83 EC 40 C5 F8 29 74 24 30 C5 F8 29 7C 24 20 C5 F8 28 FA", 0x2be3080);
        return addr;
    }

    inline uintptr_t NiCameraSetViewport()
    {
        static auto addr = FuncRelocation("48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 20 48 8B FA 48 8B F1 48 8D 99 08", 0x2be3140);
        return addr;
    }

    inline uintptr_t NiCameraSetScissors()
    {
        static auto addr = FuncRelocation("48 89 5C 24 10 48 89 6C 24 18 56 57 41 56 48 83 EC 40 48 8B FA 48 8B F1 48 8D", 0x2be31f0);
        return addr;
    }

    inline uintptr_t NiCameraSetClipspaceType()
    {
        static auto addr = FuncRelocation("48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 20 0F B6 F2 48 8B F9 48 8D 99", 0x2be3350);
        return addr;
    }

    inline uintptr_t CameraViewRegister()
    {
        static auto addr = FuncRelocation("48 89 5C 24 18 55 56 57 41 56 41 57 48 83 EC 50 4C 8B F9", 0x7c9be0);
        return addr;
    }

    inline uintptr_t WriteCameraViewData()
    {
        static auto addr = FuncRelocation(
            "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 41 56 41 57 48 83 EC 20 48 8B 05 ? ? ? ? 48 8B F2 8B 39 81 E7 FF FF FF 00 48 8B A8 20", 0x841310);
        return addr;
    }

    inline uintptr_t WriteAverageLuminanceReadback()
    {
        static auto addr = FuncRelocation("48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 41 56 41 57 48 83 EC 20 48 8B 05 ? ? ? ? 48 8B F2 8B 39 81 E7 FF FF FF 00 4C "
                                          "8B B8 F8 00 00 00 E8 ? ? ? ? 49 8D 8F C0 00 00 00 8B D0 E8 ? ? ? ? 49 8B 8F C8 02 00 00 48 8D 1C BD 00 00 00 00 44 8B C7 48 8B D0 48",
                                          0x841200);
        return addr;
    }

    inline uintptr_t WriteImageSpaceData()
    {
        static auto addr = InstructionRelocation("E8 ? ? ? ? 48 8B 05 ? ? ? ? 8B 80 D4", 1, 5, 0x8415c0);
        return addr;
    }

    inline uintptr_t WriteDirectionalShadowData()
    {
        static auto addr = InstructionRelocation("E8 ? ? ? ? 90 48 8B 8C 24 28 01 00 00 48 85 C9 74 05", 1, 5, 0xd77e70);
        return addr;
    }

    inline uintptr_t WriteHighlightSettings()
    {
        static auto addr = InstructionRelocation("E8 ? ? ? ? 48 8B 05 ? ? ? ? 48 8B B8 30 01 00 00 48 8D 9F D0 00 00 00 48 8B CB E8 ? ? ? ? 90", 1, 5, 0x15fbb30);
        return addr;
    }

    // Returns the view's FeatureSetup row prepared for writing; the caller commits the column.
    inline uintptr_t BeginWriteFeatureSetup()
    {
        static auto addr = InstructionRelocation("E8 ? ? ? ? 48 8D 8D E0 01 00 00 48 8B", 1, 5, 0x298c1f0);
        return addr;
    }

    inline uintptr_t StorageThreadSlot()
    {
        static auto addr = InstructionRelocation("E8 ? ? ? ? 8B D0 48 8D 8B C0 00 00 00 E8 ? ? ? ? 48 8B D0 48 8B CB E8 ? ? ? ? 48 8B 05 ? ? ? ? 48 8B B8 40 01 00 00 E8 "
                                                 "? ? ? ? 48 8D 8F C0 00 00 00 8B D0 E8 ? ? ? ? 44 8B C5",
                                                 1, 5, 0x7c7330);
        return addr;
    }

    inline uintptr_t StorageColumnWriter()
    {
        static auto addr = FuncRelocation("48 89 5C 24 18 48 89 6C 24 20 56 57 41 54 41 56 41 57 48 83 EC 30 8B", 0x7c7440);
        return addr;
    }

    inline uintptr_t StorageColumnCommit()
    {
        static auto addr = FuncRelocation("48 83 EC 18 83 42 08 FF 4C 8B CA 4C", 0x7c6c50);
        return addr;
    }

    // Writes a render graph's MultiCameraViewData: the camera views it renders each frame.
    inline uintptr_t SetMultiCameraViewData()
    {
        static auto addr = InstructionRelocation("E8 ? ? ? ? 41 8B 5C 24 30", 1, 5, 0x2914b90);
        return addr;
    }

    inline uintptr_t GameAllocate()
    {
        static auto addr = FuncRelocation("40 53 48 83 EC 20 83 3D ? ? ? ? 02 48 8B D9 74 13 48 8D 15 ? ? ? ? 48 8D 0D ? ? ? ? E8 ? ? ? ? 41 B1 01 48 8D 0D ? ? ? ? 41",
                                          0x316c00);
        return addr;
    }

    inline uintptr_t SmallAllocate()
    {
        static auto addr = FuncRelocation("40 53 57 41 54 41 55 41 57 48 83 EC 30 83", 0x22c7320);
        return addr;
    }

    inline uintptr_t FixedStringCreate()
    {
        static auto addr = FuncRelocation("44 88 44 24 18 48 89 54 24 10 48 89 4C 24 08 55 53 56 57 41 54 41 55 41 56 41 57 48 8D 6C 24 E1 48 81 EC 98 00 00 00 41 0F B6 F8 4C "
                                          "8B F2 E8 ? ? ? ? 48 8B F0 45",
                                          0x28cba80);
        return addr;
    }

    inline uintptr_t FixedStringRelease()
    {
        static auto addr = FuncRelocation("41 54 48 83 EC 40 48 83 39 00 4C 8B", 0x28caea0);
        return addr;
    }

    inline uintptr_t RenderGraphRegister()
    {
        static auto addr = FuncRelocation("48 89 5C 24 18 48 89 6C 24 20 56 57 41 54 41 56 41 57 48 83 EC 50 4C 8B E1 4C", 0x7ca380);
        return addr;
    }

    // Adds a {context, flags} job to a render graph submission record.
    inline uintptr_t AddRenderGraphJob()
    {
        static auto addr = FuncRelocation("48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 30 49 8B F8 8B F2 48 8B", 0x2912190);
        return addr;
    }

    // Queues a render graph submission record for this frame.
    inline uintptr_t SubmitRenderGraph()
    {
        static auto addr = FuncRelocation("48 89 5C 24 08 57 48 83 EC 30 48 8B FA 48 8B D9 48 8B 52 70", 0x29123a0);
        return addr;
    }

    inline uintptr_t WriteRenderGraphOptions()
    {
        static auto addr = InstructionRelocation("E8 ? ? ? ? 89 5C 24 30 C6", 1, 5, 0x841410);
        return addr;
    }

    inline uintptr_t WriteRenderGraphFrameFlag()
    {
        static auto addr = InstructionRelocation(
            "E8 ? ? ? ? E8 ? ? ? ? 48 8B 10 48 85 D2 74 0A B8 01 00 00 00 F0 0F C1 42 10 48 89 54 24 40 C6", 1, 5, 0x841500);
        return addr;
    }

    // Writes a render graph's sort key and name.
    inline uintptr_t WriteRenderGraphKey()
    {
        static auto addr = FuncRelocation("48 89 5C 24 10 48 89 6C 24 18 48 89 74 24 20 57 41 56 41 57 48 83 EC 20 48 8B F2 8B 39 48 8B 05 ? ? ? ? 4C 8B B8 28",
                                          0x342ce0);
        return addr;
    }

    inline uintptr_t CameraViewStorage()
    {
        static auto addr = InstructionRelocation("48 8B 05 ? ? ? ? 48 8B F2 8B 39 81 E7 FF FF FF 00 48 8B A8 20", 3, 7, 0x5975168);
        return addr;
    }

    inline uintptr_t RenderGraphStorage()
    {
        static auto addr = InstructionRelocation("48 8B 05 ? ? ? ? 8B D3 81 E2 FF FF FF 00 4C", 3, 7, 0x5975188);
        return addr;
    }

    inline uintptr_t AverageLuminanceReadbackVtable()
    {
        static auto addr = InstructionRelocation("48 8D 1D ? ? ? ? 48 85 C0 74 19", 3, 7, 0x4b48c98);
        return addr;
    }
} // namespace Steam::MemoryOffsets::Stereo
