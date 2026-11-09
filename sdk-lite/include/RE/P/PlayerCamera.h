#pragma once

#include <RE/F/FirstPersonState.h>
#include "CreationEngine/memory/offsets.h"
#include "REL/Relocation.h"

namespace RE
{
	class TESCameraState;

	struct CameraStates
	{
		enum CameraState : std::uint32_t
		{
			kFirstPerson = 0,
			kAutoVanity,
			kVATS,
			kIronSights,
			kPCTransition,
			kTween,
			kUnk06,
			kVehicleCamera,
			kFlightCamera,
			kShipFarTravel,
			kShipAction,
			kShipTargeting,
			kShipCombatOrbit,
			kFreeWalk,
			kFreeAdvanced,
			kFreeFly,
			kFreeTethered,
			kDialogue,
			kWorkshopIsometric,
			kPhotoMode,
			kThirdPerson,
			kFurniture,
			kHorse,
			kBleedout
		};
	};
	using CameraState = CameraStates::CameraState;

	class PlayerCamera
	{
	public:

		virtual ~PlayerCamera() = default;  // 00

		[[nodiscard]] inline bool IsInFirstPerson() const {
        return QCameraEquals(CameraState::kFirstPerson);
    }
        NiNode*       sceneNode;                  //0x0008 BSSceneNode
        TESCameraState* currentState;
        char 			   pad8[0x170];                     //0x0170
		FirstPersonState*      pFirstPersonModeState;         //0x0188
		TESCameraState*   pVanityState;                  //0x0178
		TESCameraState*   pVatsCameraState;              //0x0180
		TESCameraState*   pIronSightState;               //0x0188
		TESCameraState*   pPlayerCameraTransitionState;  //0x0190
		TESCameraState*   pTweenMenuCameraState;         //0x0198
		TESCameraState*   pThirdPersonState;             //0x01A0
		TESCameraState*   pVehicleCameraState;            //0x01A8
		TESCameraState*   pFlightCameraState;            //0x01A8
		TESCameraState*   pShipFarCameraState;           //0x01B0
		TESCameraState*   pShipActionCameraState;        //0x01B8
		TESCameraState*   pShipTargetingCameraState;     //0x01C0
		TESCameraState*   pShipCameraOrbitState;         //0x01C8
		TESCameraState*   pFreeWalkCameraState;          //0x01D0
		TESCameraState*   pFreeAdvancedCameraState;      //0x01D8
		TESCameraState*   pFreeFlyCameraState;           //0x01E0
		TESCameraState*   pFreeTetheredCameraState;      //0x01E8
		TESCameraState*   pDialogueCameraSTate;          //0x01F0
		TESCameraState*   pWorkshopIsoCameraState;       //0x01F8
		TESCameraState*   pPhotoModeCameraState;         //0x0200
		TESCameraState*   pThirdPersonState2;            //0x0208
		TESCameraState*   pFurnitureCameraState;         //0x0210
		TESCameraState*   pHorseCameraState;             //0x0218
		TESCameraState*   pBleedoutCameraState;          //0x0220
		std::uint64_t          N00000A83;                //0x0228
		std::uint64_t     N00000A84;                     //0x0230
		std::uint64_t          N00000A85;                //0x0238
		std::uint8_t           pad_0240[8];              //0x0240
		void*             pHavokSphereShape;             //0x0248
		void*             pHavokBsWorld;                 //0x0250
		float             N00000A89;                     //0x0258
		float             N000012F9;                     //0x025C
		float             fov;                           //0x0260
		float             N00001300;                     //0x0264
		float             N00000A8B;                     //0x0268
		float             fovAdj;                        //0x026C
		float             N00000A8C;                     //0x0270
		float             N0000131E;                     //0x0274
		float             notViewFov;                    //0x0278
		float             N00001321;                     //0x027C
		float             N00000A8E;                     //0x0280
		float             N00001324;                     //0x0284
		float             N0000133D;                     //0x0288
		float             N00000A8F;                     //0x028C
		std::uint8_t      N00001340;                     //0x0290
		uint8_t           pad_0291[3];                   //0x0291
		float             N00000A90;                     //0x0294
		float             N00001345;                     //0x0298
		float             position[3];                   //0x029C
		float             horizontal_rotation;           //0x02A8
		float             N00000A93;                     //0x02AC
		float             N00000A94;                     //0x02B0
		float             N00001352;                     //0x02B4
		std::int32_t           N00000A95;                     //0x02B8
		std::int32_t           N00001355;                     //0x02BC
		std::uint16_t          N00000A96;                     //0x02C0
		std::uint8_t           N00001359;                     //0x02C2
		std::uint8_t           N0000135C;                     //0x02C3
		std::int32_t           N0000135F;                     //0x02C4
		std::int16_t           N0000135D;                     //0x02C8
		std::uint8_t           N0000135A;                     //0x02CA
		std::uint8_t           N00001365;                     //0x02CB

	private:
		[[nodiscard]] inline bool QCameraEquals(CameraState a_cameraState) const {
            auto states = &this->pFirstPersonModeState;
            auto state = states[static_cast<std::size_t>(a_cameraState)];
            // both are null in the main menu; null == null must not count as "in this state"
            return state && (uintptr_t ) state == (uintptr_t )this->currentState;
        }
	};
	static_assert(offsetof(PlayerCamera, pFirstPersonModeState) == 0x188);
//	static_assert(offsetof(PlayerCamera, N000012DD) == 0xA8);
	static_assert(offsetof(PlayerCamera, horizontal_rotation) == 0x2C8);
	static_assert(offsetof(PlayerCamera, fov) == 0x280);
	static_assert(sizeof(PlayerCamera) == 0x2F0);
}
