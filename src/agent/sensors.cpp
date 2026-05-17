#include "sensors.hpp"
#include <iostream>
#include <vector>

namespace wspa {
    SensorManager* SensorManager::s_instance = nullptr;

    SensorManager::SensorManager(TagDatabase& db) : m_db(db), m_hook(nullptr), m_running(false) {
        s_instance = this;
    }

    SensorManager::~SensorManager() {
        stop();
    }

    void SensorManager::start() {
        m_running = true;
        
        // Start the Interrupt Lane: Foreground Window Tracking
        m_hook = SetWinEventHook(
            EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
            NULL, win_event_proc, 0, 0, WINEVENT_OUTOFCONTEXT);

        if (!m_hook) {
            std::cerr << "[Sensors] Failed to set WinEventHook" << std::endl;
        } else {
            std::cout << "[Sensors] Foreground window hook established." << std::endl;
        }

        // In a real production app, we would start a coalesced timer here for the Coalesced Lane.
        // For this scaffold, we'll simulate it in the main loop or a separate thread.
    }

    void SensorManager::stop() {
        if (m_hook) {
            UnhookWinEvent(m_hook);
            m_hook = nullptr;
        }
        m_running = false;
    }

    void CALLBACK SensorManager::win_event_proc(
        HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd,
        LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime) {
        
        if (event == EVENT_SYSTEM_FOREGROUND && s_instance) {
            char windowTitle[256];
            GetWindowTextA(hwnd, windowTitle, sizeof(windowTitle));
            
            s_instance->m_db["Foreground_App"] = { 
                std::string(windowTitle), 
                std::chrono::system_clock::now() 
            };
            
            std::cout << "[Sensors] Foreground changed: " << windowTitle << std::endl;
        }
    }

    void SensorManager::collect_performance_metrics() {
        // Placeholder for PDH or GetSystemTimes usage
        // Example: Capture CPU utilization
        FILETIME idleTime, kernelTime, userTime;
        if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
            // Logic to calculate delta and update DB
            // m_db["CPU_Utilization"] = ...
        }
    }
}
