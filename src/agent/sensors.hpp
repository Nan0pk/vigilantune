#pragma once
#include <windows.h>
#include <pdh.h>
#include <string>
#include <functional>
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

        bool m_running;
        static SensorManager* s_instance;
    };
}
