#pragma once
#include <windows.h>
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

    private:
        static void CALLBACK win_event_proc(
            HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd,
            LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime);

        void collect_performance_metrics();

        TagDatabase& m_db;
        HWINEVENTHOOK m_hook;
        bool m_running;
        static SensorManager* s_instance;
    };
}
