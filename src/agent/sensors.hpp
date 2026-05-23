#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <pdh.h>
#include <string>
#include <functional>
#include <atomic>
#include <thread>
#include "../shared/types.hpp"
#include "../shared/scoped_handle.hpp"
#include "gpu_telemetry.hpp"

namespace vigilantune {
    class SensorManager {
    public:
        SensorManager(TagDatabase& db);
        ~SensorManager();

        void start();
        void stop();

        void collect_performance_metrics();
        void collect_high_fidelity_metrics();

    private:
        static void CALLBACK win_event_proc(
            HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd,
            LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime);

        void aux_telemetry_thread_proc();
        double get_primary_drive_temperature();
        double get_network_throughput();
        void collect_battery_metrics();
        void collect_memory_metrics();

        TagDatabase& m_db;
        nanoloop::ScopedWinEventHook m_hook;
        
        // PDH members
        nanoloop::ScopedPdhQuery m_query;
        PDH_HCOUNTER m_cpu_counter;
        PDH_HCOUNTER m_queue_counter;
        PDH_HCOUNTER m_disk_counter;
        PDH_HCOUNTER m_gpu_counter;
        PDH_HCOUNTER m_thermal_counter;

        std::atomic<bool> m_running;
        std::atomic<int> m_active_callbacks{0};
        static std::atomic<SensorManager*> s_instance;

        std::thread m_aux_thread;

        struct PROCESSOR_POWER_INFORMATION {
            ULONG  Number;
            ULONG  MaxMhz;
            ULONG  CurrentMhz;
            ULONG  MhzLimit;
            ULONG  MaxIdleState;
            ULONG  CurrentIdleState;
        };

        GpuTelemetryLoader m_gpu_loader;
    };
} // namespace vigilantune
