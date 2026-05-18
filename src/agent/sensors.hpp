#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <pdh.h>
#include <string>
#include <functional>
#include <atomic>
#include "../shared/types.hpp"

namespace wspa {
    class SensorManager {
    public:
        SensorManager(TagDatabase& db);
        ~SensorManager();

        void start();
        void stop();

        // New: Public method to trigger a collection cycle
        void collect_performance_metrics();
        void collect_high_fidelity_metrics(); // Gap #4, #8

    private:
        static void CALLBACK win_event_proc(
            HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd,
            LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime);

        TagDatabase& m_db;
        HWINEVENTHOOK m_hook;
        
        // PDH members
        PDH_HQUERY m_query;
        PDH_HCOUNTER m_cpu_counter;
        PDH_HCOUNTER m_queue_counter;
        PDH_HCOUNTER m_disk_counter;
        PDH_HCOUNTER m_gpu_counter;
        PDH_HCOUNTER m_thermal_counter;

        std::atomic<bool> m_running;
        std::atomic<int> m_active_callbacks{0};
        static std::atomic<SensorManager*> s_instance;

        // Gap #4 Structures
        struct PROCESSOR_POWER_INFORMATION {
            ULONG  Number;
            ULONG  MaxMhz;
            ULONG  CurrentMhz;
            ULONG  MhzLimit;
            ULONG  MaxIdleState;
            ULONG  CurrentIdleState;
        };
    };
}
