#pragma once

namespace RE
{
    class NiAVObject;
}

// Third-person body shown in first person with arm, leg and weapon IK.
namespace body
{
    // Hooks BSModelNode::UpdateTransforms, where the IK is applied to the body's pose buffer.
    void InstallModelNodeHook();
    // Called for each object as it updates its world transform; toggles the body at its 3P root.
    void OnUpdateWorld(RE::NiAVObject* obj, int engine_frame);
    // Muzzle of the weapon held by the shown body, or null when the body is not driving the weapon.
    RE::NiAVObject* GetBodyMuzzle();
    // Aim direction (game world) the held weapon was posed along this frame, if any.
    bool GetBodyAimForward(float out[3]);
    // The player fired: the weapon's pose in the first-person rig is sampled as its firing grip.
    void NotifyPlayerFired();
}
