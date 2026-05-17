#include "sensors.hpp"
#include <iostream>
#include <vector>

#pragma comment(lib, "pdh.lib")

namespace wspa {
    SensorManager* SensorManager::s_instance = nullptr;

    SensorManager::SensorManager(TagDatabase& db) 
        : m_db(db), m_hook(nullptr), m_running(false), m_query(nullptr) {
        s_instance = this;
    }

    SensorManager::~SensorManager() {
        stop();
    }

    void SensorManager::start() {
        m_running = true;
        
        // 1. Interrupt Lane: Foreground Window Tracking
        m_hook = SetWinEventHook(
            EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
            NULL, win_event_proc, 0, 0, WINEVENT_OUTOFCONTEXT);

        // 2. Coalesced Lane: Initialize PDH for performance metrics
        if (PdhOpenQuery(NULL, 0, &m_query) == ERROR_SUCCESS) {
            PdhAddCounter(m_query, "\\Processor(_Total)\\% Processor Time", 0, &m_cpu_counter);
            PdhAddCounter(m_query, "\\System\\Processor Queue Length", 0, &m_queue_counter);
            PdhAddCounter(m_query, "\\PhysicalDisk(_Total)\\% Disk Time", 0, &m_disk_counter);
            
            // Note: GPU counters can vary. This is a common path for WDDM 2.0+
            // Using a wildcard for GPU engine utilization
            PdhAddCounter(m_query, "\\GPU Engine(*)\\Utilization Percentage", 0, &m_gpu_counter);
            
            PdhCollectQueryData(m_query); // Initial collection
        } else {
            std::cerr << "[Sensors] Failed to open PDH query" << std::endl;
        }

        std::cout << "[Sensors] Telemetry lanes initialized." << std::endl;
    }

    void SensorManager::stop() {
        if (m_hook) {
            UnhookWinEvent(m_hook);
            m_hook = nullptr;
        }
        if (m_query) {
            PdhCloseQuery(m_query);
            m_query = nullptr;
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
        }
    }

    void SensorManager::collect_performance_metrics() {
        if (!m_query) return;

        if (PdhCollectQueryData(m_query) == ERROR_SUCCESS) {
            PDH_FMT_COUNTERVALUE cpu_val;
            PDH_FMT_COUNTERVALUE queue_val;
            PDH_FMT_COUNTERVALUE disk_val;
            PDH_FMT_COUNTERVALUE gpu_val;

            if (PdhGetFormattedCounterValue(m_cpu_counter, PDH_FMT_DOUBLE, NULL, &cpu_val) == ERROR_SUCCESS) {
                m_db["CPU_Utilization"] = { cpu_val.doubleValue, std::chrono::system_clock::now() };
            }

            if (PdhGetFormattedCounterValue(m_queue_counter, PDH_FMT_LONG, NULL, &queue_val) == ERROR_SUCCESS) {
                m_db["Thread_Queue_Length"] = { (int)queue_val.longValue, std::chrono::system_clock::now() };
            }

            if (PdhGetFormattedCounterValue(m_disk_counter, PDH_FMT_DOUBLE, NULL, &disk_val) == ERROR_SUCCESS) {
                m_db["Disk_Utilization"] = { disk_val.doubleValue, std::chrono::system_clock::now() };
            }

            // GPU is often a sum of multiple engines; simplified here
            if (PdhGetFormattedCounterValue(m_gpu_counter, PDH_FMT_DOUBLE, NULL, &gpu_val) == ERROR_SUCCESS) {
                m_db["GPU_Utilization"] = { gpu_val.doubleValue, std::chrono::system_clock::now() };
            }
        }
    }
}
