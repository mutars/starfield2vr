#pragma once

#include <cstdint>

// Periodic frame-time summary in vr_log.txt: present intervals, CPU stage times from the game's Reflex markers, and
// GPU times from timestamps written inside render passes.
namespace PerfStats
{
    void OnPresent();

    // Reflex marker (0 simulation start, 1 simulation end, 2 render submit start, 3 render submit end, 4 present start,
    // 5 present end) for an engine frame.
    void OnReflexMarker(uint32_t marker, uint32_t frame);

    // Points in each eye's GPU work, recorded inside the DLSS and post effect passes, whose command lists are recording.
    // The right eye's graph is submitted first, so its DLSS inputs open each frame.
    enum class GpuPoint
    {
        kEyeUpscaleStart, // the eye's DLSS inputs, before they run
        kEyeUpscaleEnd,   // the eye's DLSS upscale, after it runs
        kEyePostEnd,      // the eye's last post effect, after it runs
    };
    void MarkGpu(void* command_list, GpuPoint point, int eye);
}
