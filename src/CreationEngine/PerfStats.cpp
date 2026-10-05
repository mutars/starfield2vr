#include "PerfStats.h"

#include <Framework.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <d3d12.h>
#include <mutex>
#include <vector>
#include <wrl/client.h>

#include <mods/VR.hpp>

namespace PerfStats
{
    namespace
    {
        using clock = std::chrono::steady_clock;

        constexpr auto kReportInterval = std::chrono::seconds(5);

        clock::time_point  g_last_present{};
        clock::time_point  g_window_start{};
        std::vector<float> g_frame_ms;

        // A running average.
        struct Average
        {
            double sum{ 0.0 };
            int    count{ 0 };
            void   add(double value)
            {
                sum += value;
                ++count;
            }
            double take()
            {
                const double value = count > 0 ? sum / count : 0.0;
                sum                = 0.0;
                count              = 0;
                return value;
            }
        };

        // CPU stages from the Reflex markers, matched per engine frame.
        constexpr int kMarkerFrames = 8;
        struct MarkerFrame
        {
            uint32_t                         frame{ 0xFFFFFFFF };
            std::array<clock::time_point, 6> at{};
            uint32_t                         seen{ 0 };
        };
        std::mutex                              g_marker_mutex;
        std::array<MarkerFrame, kMarkerFrames>  g_marker_frames{};
        Average                                 g_cpu_simulation, g_cpu_submit, g_cpu_present;

        // GPU timestamps: per frame slot, three points per eye.
        constexpr int kSlots   = 4;
        constexpr int kPerSlot = 6;
        constexpr int kQueries = kSlots * kPerSlot;
        std::mutex                                g_gpu_mutex;
        Microsoft::WRL::ComPtr<ID3D12QueryHeap>   g_query_heap;
        Microsoft::WRL::ComPtr<ID3D12Resource>    g_readback;
        uint64_t                                  g_timestamp_hz{ 0 };
        bool                                      g_gpu_failed{ false };
        std::atomic<uint32_t>                     g_gpu_sequence{ 0 };
        std::array<std::atomic<uint32_t>, kSlots> g_written{};
        std::array<uint32_t, kSlots>              g_slot_sequence{};
        Average                                   g_gpu_eyes;
        std::array<Average, 2>                    g_gpu_upscale, g_gpu_post;

        int QueryIndex(PerfStats::GpuPoint point, int eye) { return (eye & 1) * 3 + (int)point; }

        bool EnsureQueries()
        {
            if (g_query_heap != nullptr) {
                return true;
            }
            if (g_gpu_failed) {
                return false;
            }
            auto& hook   = g_framework->get_d3d12_hook();
            auto  device = hook ? hook->get_device() : nullptr;
            auto  queue  = hook ? hook->get_command_queue() : nullptr;
            if (device == nullptr || queue == nullptr) {
                return false;
            }
            D3D12_QUERY_HEAP_DESC heap_desc{ D3D12_QUERY_HEAP_TYPE_TIMESTAMP, kQueries, 0 };
            D3D12_HEAP_PROPERTIES heap{ D3D12_HEAP_TYPE_READBACK };
            D3D12_RESOURCE_DESC   desc{};
            desc.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
            desc.Width            = kQueries * sizeof(uint64_t);
            desc.Height           = 1;
            desc.DepthOrArraySize = 1;
            desc.MipLevels        = 1;
            desc.SampleDesc.Count = 1;
            desc.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            if (FAILED(queue->GetTimestampFrequency(&g_timestamp_hz)) || FAILED(device->CreateQueryHeap(&heap_desc, IID_PPV_ARGS(&g_query_heap))) ||
                FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&g_readback)))) {
                spdlog::warn("[Perf] GPU timestamps unavailable");
                g_query_heap.Reset();
                g_readback.Reset();
                g_gpu_failed = true;
                return false;
            }
            return true;
        }

        // Slots at least two frames behind the GPU's current frame have finished; their spans join the averages.
        void CollectGpu()
        {
            std::scoped_lock _{ g_gpu_mutex };
            if (g_readback == nullptr || g_timestamp_hz == 0) {
                return;
            }
            const auto current = g_gpu_sequence.load();
            for (int slot = 0; slot < kSlots; ++slot) {
                const auto written = g_written[slot].load();
                if (written == 0 || current - g_slot_sequence[slot] < 2) {
                    continue;
                }
                uint64_t*         data = nullptr;
                const D3D12_RANGE range{ slot * kPerSlot * sizeof(uint64_t), (slot + 1) * kPerSlot * sizeof(uint64_t) };
                if (FAILED(g_readback->Map(0, &range, reinterpret_cast<void**>(&data)))) {
                    continue;
                }
                const auto base = data + slot * kPerSlot;
                auto       span = [&](int from, int to, Average& average) {
                    if ((written & (1u << from)) && (written & (1u << to)) && base[to] > base[from]) {
                        average.add((double)(base[to] - base[from]) * 1000.0 / (double)g_timestamp_hz);
                    }
                };
                for (int eye = 0; eye < 2; ++eye) {
                    const int e = eye * 3;
                    span(e + 0, e + 1, g_gpu_upscale[eye]);
                    span(e + 1, e + 2, g_gpu_post[eye]);
                }
                // From the right eye's DLSS to the end of the left eye's post chain: the left eye's scene rendering
                // and both eyes' upscale and post work.
                span(3, 2, g_gpu_eyes);
                const D3D12_RANGE nothing{ 0, 0 };
                g_readback->Unmap(0, &nothing);
                g_written[slot] = 0;
            }
        }

        void Report(float seconds)
        {
            if (g_frame_ms.empty()) {
                return;
            }
            std::sort(g_frame_ms.begin(), g_frame_ms.end());
            const auto count = g_frame_ms.size();
            float      sum   = 0.0f;
            for (auto ms : g_frame_ms) {
                sum += ms;
            }
            const float fps = count / seconds;
            const float p99 = g_frame_ms[std::min(count - 1, count * 99 / 100)];

            // Every frame carries both eyes, so the game must deliver one frame per display refresh.
            const double period    = VR::get()->get_display_period();
            const float  budget_ms = period > 0.0 ? (float)(period * 1000.0) : 0.0f;
            std::size_t  over      = 0;
            if (budget_ms > 0.0f) {
                over = count - (std::upper_bound(g_frame_ms.begin(), g_frame_ms.end(), budget_ms * 1.1f) - g_frame_ms.begin());
            }

            double simulation, submit, present;
            {
                std::scoped_lock _{ g_marker_mutex };
                simulation = g_cpu_simulation.take();
                submit     = g_cpu_submit.take();
                present    = g_cpu_present.take();
            }
            double eyes;
            std::array<double, 2> upscale{}, post{};
            {
                std::scoped_lock _{ g_gpu_mutex };
                eyes = g_gpu_eyes.take();
                for (int eye = 0; eye < 2; ++eye) {
                    upscale[eye] = g_gpu_upscale[eye].take();
                    post[eye]    = g_gpu_post[eye].take();
                }
            }

            spdlog::info("[Perf] {:.1f} fps | frame avg {:.1f} ms, p99 {:.1f} ms, max {:.1f} ms | budget {:.1f} ms ({:.0f} Hz), {:.0f}% over", fps, sum / count, p99,
                g_frame_ms.back(), budget_ms, period > 0.0 ? 1.0 / period : 0.0, 100.0f * over / count);
            spdlog::info("[Perf] CPU: simulation {:.1f} ms, render submit {:.1f} ms, present {:.1f} ms | GPU: right DLSS to left post end {:.1f} ms; "
                         "left eye DLSS {:.1f}, post {:.1f} ms; right eye DLSS {:.1f}, post {:.1f} ms",
                simulation, submit, present, eyes, upscale[0], post[0], upscale[1], post[1]);

            g_frame_ms.clear();
        }
    }

    void OnPresent()
    {
        const auto now = clock::now();
        if (g_last_present.time_since_epoch().count() == 0) {
            g_last_present = now;
            g_window_start = now;
            g_frame_ms.reserve(2048);
            return;
        }

        CollectGpu();
        g_frame_ms.push_back(std::chrono::duration<float, std::milli>(now - g_last_present).count());
        g_last_present = now;

        if (now - g_window_start >= kReportInterval) {
            Report(std::chrono::duration<float>(now - g_window_start).count());
            g_window_start = now;
        }
    }

    void OnReflexMarker(uint32_t marker, uint32_t frame)
    {
        if (marker > 5) {
            return;
        }
        const auto now = clock::now();
        std::scoped_lock _{ g_marker_mutex };
        auto& entry = g_marker_frames[frame % kMarkerFrames];
        if (entry.frame != frame) {
            entry       = {};
            entry.frame = frame;
        }
        entry.at[marker] = now;
        entry.seen |= 1u << marker;
        auto stage = [&](uint32_t from, uint32_t to, Average& average) {
            if (marker == to && (entry.seen & (1u << from))) {
                average.add(std::chrono::duration<double, std::milli>(entry.at[to] - entry.at[from]).count());
            }
        };
        stage(0, 1, g_cpu_simulation);
        stage(2, 3, g_cpu_submit);
        stage(4, 5, g_cpu_present);
    }

    void MarkGpu(void* command_list, GpuPoint point, int eye)
    {
        auto list = static_cast<ID3D12GraphicsCommandList*>(command_list);
        if (list == nullptr || list->GetType() == D3D12_COMMAND_LIST_TYPE_COPY || !EnsureQueries()) {
            return;
        }
        // The right eye's DLSS inputs open a new slot; every later point of the frame lands in it.
        uint32_t sequence;
        if (point == GpuPoint::kEyeUpscaleStart && eye == 1) {
            sequence = g_gpu_sequence.fetch_add(1) + 1;
            std::scoped_lock _{ g_gpu_mutex };
            g_written[sequence % kSlots]       = 0;
            g_slot_sequence[sequence % kSlots] = sequence;
        } else {
            sequence = g_gpu_sequence.load();
        }
        const int slot  = (int)(sequence % kSlots);
        const int index = slot * kPerSlot + QueryIndex(point, eye);
        list->EndQuery(g_query_heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, index);
        list->ResolveQueryData(g_query_heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, index, 1, g_readback.Get(), index * sizeof(uint64_t));
        g_written[slot] |= 1u << QueryIndex(point, eye);
    }
}
