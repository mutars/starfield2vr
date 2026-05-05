#pragma once
#include "ScanHelper.h"
#include "offsets_table.h"

using namespace MemoryScan;

namespace Steam::MemoryOffsets
{
    namespace Setting
    {
//        inline uintptr_t INIPrefSettingCollection()
//        {
//            // ID 885866
//            auto val = (uintptr_t)mod + OffsetsTable::GetOffset(885866);
//            return val;
//        }

//        inline uintptr_t INISettingCollection()
//        {
//            // ID 885862
//            // .... need to find by vtable
//            auto val = (uintptr_t)mod + OffsetsTable::GetOffset(885862);
//            return val;
//        }
    } // namespace Setting

    namespace FirstPersonState {
        inline uintptr_t GetRotationQuatV() {
            static auto pattern = ".?AVFirstPersonState@@";
            static auto addr = ((uintptr_t*)VTable("FirstPersonState::vftable[13]", pattern, OffsetsTable::GetOffset(459617)))[13];
            return addr;
        }

//        inline uintptr_t GetLocationV() {
//            static auto pattern = ".?AVFirstPersonState@@";
//            static auto addr = ((uintptr_t*)VTable("FirstPersonState::vftable[14]", pattern, OffsetsTable::GetOffset(459617)))[14];
//            return addr;
//        }
    }

    namespace BSFadeNode {
        inline uintptr_t vtable_UpdateWorld() {
            static auto pattern = ".?AVBSFadeNode@@";
            static auto addr = ((uintptr_t*)VTable("BSFadeNode::vftable", pattern, OffsetsTable::GetOffset(472039)))[79];
            return addr;
        }
    }

//    namespace BSSceneNode {
//        inline uintptr_t vtable_Update() {
//            static auto pattern = ".?AVBSSceneNode@@";
//            static auto addr = ((uintptr_t*)VTable("BSSceneNode::vftable", pattern, 0x55ad438))[73];
//            return addr;
//        }
//    }
//
//    namespace NiNode {
//        inline uintptr_t vtable_UpdateWorld() {
//            static auto pattern = ".?AVNiNode@@";
//            static auto addr = ((uintptr_t*)VTable("NiNode::vftable", pattern, 0x37DE584))[79];
//            return addr;
//        }
//    }

    namespace NiCamera
    {
        inline uintptr_t SetFrustumVfunc()
        {
            // ID 147392
            auto        pattern     = "48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 20 48 8B F2 48 8B F9 48 8D 99 08";
            static auto address_ptr = FuncRelocation(pattern, OffsetsTable::GetOffset(147392), 147392);
            return address_ptr;
        }

        inline uintptr_t CalcFrustumVfunc()
        {
            // ID 147416
            auto        pattern     = "48 8B C4 48 89 58 10 48 89 70 18 57 48 81 EC D0 00 00 00 C5 F8 29 70 E8 C5 F8 29 78 D8 C5 78 29 40 C8 C5 78 29 48 B8 C5 78 28";
            static auto address_ptr = FuncRelocation(pattern, OffsetsTable::GetOffset(147416), 147416);
            return address_ptr;
        }

//        inline uintptr_t UpdateWorldVfunc()
//        {
//            // ID 210425
//            auto        pattern     = "48 8B C4 48 89 58 10 55 56 57 41 54 41 55 41 56 41 57 48 8D A8 E8 FE FF FF 48 81 EC E0 01 00 00 C5 F8 29 70 B8 C5 F8 29 78 A8 C5";
//            static auto address_ptr = FuncRelocation(pattern, OffsetsTable::GetOffset(210425), 210425);
//            return address_ptr;
//        }
//
//        inline uintptr_t Vtable()
//        {
//            // ID ??
//            auto        pattern     = "48 8D 05 ? ? ? ? 48 89 06 C7 86 30 01 00 00 FF FF FF 00";
//            static auto address_ptr = InstructionRelocation(pattern, 3, 7, OffsetsTable::GetOffset(0), 0);
//            return address_ptr;
//        }
    } // namespace NiCamera

    namespace NiAVObject
    {
//        inline uintptr_t UpdateWorld()
//        {
//            // ID 210234
//            auto        pattern     = "48 89 5C 24 08 57 48 83 EC 20 F6 42 38 01 48 8B DA";
//            static auto address_ptr = FuncRelocation(pattern, OffsetsTable::GetOffset(210234), 210234);
//            return address_ptr;
//        }
    } // namespace NiAVObject

    namespace Gamepad
    {
        inline uintptr_t PollVfunc()
        {
            // ID 179249
            static auto pattern = ".?AVBSPCGamepadDevice@@";
            static auto addr = ((uintptr_t*)VTable("BSPCGamepadDevice::vftable[2]", pattern, OffsetsTable::GetOffset(470133)))[2];
            return addr;
        }
    } // namespace Gamepad

    namespace Scaleform::Movie
    {
        inline uintptr_t SetViewportVFunc()
        {
            // ID 50603
            auto        pattern     = ".?AVMovieImpl@GFx@Scaleform@@";
            static auto addr = ((uintptr_t*)VTable("Scaleform::GFx::MovieImpl::vftable[12]", pattern, OffsetsTable::GetOffset(303817)))[12];
            return addr;
        }
    } // namespace Scaleform::Movie

    namespace CreationRenderer
    {
        inline uintptr_t RenderGraphFrameStart()
        {
            // ID 143812
            auto        pattern     = "48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 18 55 41 54 41 55 41 56 41 57 48 81 EC F0";
            static auto address_ptr = FuncRelocation(pattern, OffsetsTable::GetOffset(143812), 143812);
            return address_ptr;
        }

//        inline uintptr_t RenderGraphRenderPipelineExecute()
//        {
//            // ID 202136
//            auto        pattern     = "40 53 48 83 EC 20 48 8B 91 58 02 00 00 4C 8D 41 18";
//            static auto address_ptr = FuncRelocation(pattern, OffsetsTable::GetOffset(202136), 202136);
//            return address_ptr;
//        }

//        inline uintptr_t DlssUpscaleVtable()
//        {
//            // ID 1100493
//            auto pattern = "";
//            // unable to find stable
//            return 0;
//        }

        inline uintptr_t GetDXGIState()
        {
            // ID 145355
            auto        pattern     = "8B C1 44 8B C1 25 1C 58 04 00 3D 1C 58 04 00 75 06 B8 C3 0A 00 00 C3 41 F6 C0 02 0F 85 87 00 00 00";
            static auto address_ptr = FuncRelocation(pattern, OffsetsTable::GetOffset(145355), 145355);
            auto val = (uintptr_t)mod + OffsetsTable::GetOffset(145355);
            return val;
        }

        inline uintptr_t GetCommandList()
        {
            // ID 144161
            auto pattern     = "48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 20 48 8B 99 38";
            static auto address_ptr = FuncRelocation(pattern, OffsetsTable::GetOffset(144161), 144161);
            return address_ptr;
        }

        inline uintptr_t GlobalFrameCount()
        {
            // ID
            auto pattern     = "8B 15 ? ? ? ? 89 55 38 41 8B 4E 08";
            static auto address_ptr = InstructionRelocation(pattern, 2, 6, OffsetsTable::GetOffset(883900), 883900);
            return address_ptr;
        }

        inline uintptr_t OnUpdateConstantBufferView()
        {
            // ID 142800
            auto pattern     = "48 8B C4 48 89 58 10 48 89 70 18 57 41 56 41 57 48 81 EC 40";
            static auto address_ptr = FuncRelocation(pattern, OffsetsTable::GetOffset(142800), 142800);
            return address_ptr;
        }

        inline uintptr_t OnTaaVFunc7()
        {
            // ID 497712
            auto        pattern     = ".?AVTemporalAA_idTech7RenderPass@CreationRendererPrivate@@";
            static auto addr = ((uintptr_t*)VTable("CreationRendererPrivate::TemporalAA_idTech7RenderPass::vftable[7]", pattern, OffsetsTable::GetOffset(497712)))[7];
            return addr;
        }
    } // namespace CreationRenderer

    namespace Nvidia {

        inline uintptr_t onSetReflexMarkerInternal()
        {
            // ID 141825
            auto pattern     = "48 89 5C 24 10 56 48 83 EC 20 8B F2";
            auto        address_ptr = FuncRelocation(pattern, OffsetsTable::GetOffset(141825), 141825);
            return address_ptr;
        }
    }

    namespace PlayerCamera
    {
        inline uintptr_t Singleton()
        {
            // ID 937788
            auto        pattern     = "48 8B 05 ? ? ? ? 48 8B 48 10 48 85 C9 74 12 48 3B 88 D8 01 00 00";
            static auto address_ptr = InstructionRelocation(pattern, 3, 7, OffsetsTable::GetOffset(937788), 937788);
            return address_ptr;
        }

//        inline uintptr_t QCameraEquals()
//        {
//            // ID 166081
//            auto val = (uintptr_t)mod + OffsetsTable::GetOffset(166081);
//            return val;
//        }
    } // namespace PlayerCamera

    namespace Streamline
    {
        inline uintptr_t SetDlssOptions()
        {
            // ID 944432
            auto        pattern     = "48 8B 05 ? ? ? ? 48 8D 54 24 30 48 8D 4D E0 FF D0 40 38 7B 0C";
            static auto address_ptr = InstructionRelocation(pattern, 3, 7, OffsetsTable::GetOffset(944432), 944432);
            return address_ptr;
        }
    } // namespace Streamline

    namespace StorageTable::RenderGraphHandle
    {
//        inline uintptr_t Singleton()
//        {
//            // ID 772639
//            auto        pattern     = "48 8B 0D ? ? ? ? 4C 8B 89 E8 00 00 00 49 8B 49 40";
//            static auto address_ptr = InstructionRelocation(pattern, 3, 7, OffsetsTable::GetOffset(772639), 772639);
//            return address_ptr;
//        }
    } // namespace StorageTable::RenderGraphHandle

//    inline uintptr_t GlobalFrameCounter()
//    {
//        // ID 886292
//        auto        pattern     = "8B 1D ? ? ? ? 8B 81 28 01 00 00";
//        static auto address_ptr = InstructionRelocation(pattern, 2, 6, OffsetsTable::GetOffset(886292), 886292);
//        return address_ptr;
//    }
//
//    inline uintptr_t GlobalWorldFov()
//    {
//        // ID 1171750
//        auto        pattern     = "C5 FA 10 05 ? ? ? ? 84 C9 74 61";
//        static auto address_ptr = InstructionRelocation(pattern, 4, 8, OffsetsTable::GetOffset(1171750), 1171750);
//        return address_ptr;
//    }

    inline uintptr_t GlobalRenderSettings()
    {
        // ID 937583
        auto        pattern     = "48 8B 05 ? ? ? ? C5 FB 10 8E D0 01 00 00";
        static auto address_ptr = InstructionRelocation(pattern, 3, 7, OffsetsTable::GetOffset(937583), 937583);
        return address_ptr;
    }

    //TODO Only required to get command queue, may be not needed
//    inline uintptr_t GlobalDirectX12Module()
//    {
//        // ID 886000
//        auto        pattern     = "48 89 05 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? 48 8B 1F";
//        static auto address_ptr = InstructionRelocation(pattern, 3, 7, OffsetsTable::GetOffset(886000), 886000);
//        return address_ptr;
//    }

    //TODO Only required to get command queue, may be not needed
    inline uintptr_t GlobalDirectX12Module2()
    {
        // ID 944397
        auto        pattern     = "48 8B 0D ? ? ? ? 49 8B 43 28";
        static auto address_ptr = InstructionRelocation(pattern, 3, 7, OffsetsTable::GetOffset(944397), 944397);
        return address_ptr;
    }

//    inline uintptr_t GlobalAspectRatio()
//    {
//        // ID 873288
//        auto        pattern     = "C5 FA 10 05 ? ? ? ? C5 FA 10 0D ? ? ? ? C5 F8 2F D0";
//        static auto address_ptr = InstructionRelocation(pattern, 4, 8, OffsetsTable::GetOffset(873288), 873288);
//        return address_ptr;
//    }

    inline uintptr_t GlobalSceneGraphRoot()
    {
        // ID 936470
        auto        pattern     = "48 8B 05 ? ? ? ? 48 8B B8 80 00 00 00 48 8B CB";
        static auto address_ptr = InstructionRelocation(pattern, 3, 7, OffsetsTable::GetOffset(936470), 936470);
        return address_ptr;
    }

    inline uintptr_t GlobalPlayerRef()
    {
        // ID 922868
        static const char* patterns[] = {
            "48 8B 05 ? ? ? ? F6 80 2A 11 00 00 08 74 ?",
            "48 8B 05 ? ? ? ? F6 80 29 11 00 00 08 74 ?",
            "48 8B 05 ? ? ? ? F6 80 64 07 00 00 08 74 ?",
        };
        static auto address_ptr = []() -> uintptr_t {
            for (auto pattern : patterns) {
                auto result = InstructionRelocation(pattern, 3, 7, OffsetsTable::GetOffset(922868), 922868);
                if (result) return result;
            }
            return 0;
        }();
        return address_ptr;
    }
} // namespace Steam::MemoryOffsets

namespace GameStore = Steam;
