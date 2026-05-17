#include "sensors.hpp"
#include <iostream>
#include <vector>
#include <algorithm>

#pragma comment(lib, "pdh.lib")

namespace wspa {
    std::atomic<SensorManager*> SensorManager::s_instance = nullptr;

    SensorManager::SensorManager(TagDatabase& db) 
        : m_db(db), m_hook(nullptr), m_running(false), m_query(nullptr) {
        s_instance.store(this);
    }

    SensorManager::~SensorManager() {
        stop();
        s_instance.store(nullptr);
    }

    void SensorManager::start() {
        m_running = true;
        
        // 1. Interrupt Lane: Foreground Window Tracking
        m_hook = SetWinEventHook(
            EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
            NULL, win_event_proc, 0, 0, WINEVENT_OUTOFCONTEXT);

        // 2. Coalesced Lane: Initialize PDH for performance metrics
        if (PdhOpenQuery(NULL, 0, &m_query) == ERROR_SUCCESS) {
            bool any_failed = false;
            if (PdhAddCounter(m_query, "\\Processor(_Total)\\% Processor Time", 0, &m_cpu_counter) != ERROR_SUCCESS) any_failed = true;
            if (PdhAddCounter(m_query, "\\System\\Processor Queue Length", 0, &m_queue_counter) != ERROR_SUCCESS) any_failed = true;
            if (PdhAddCounter(m_query, "\\PhysicalDisk(_Total)\\% Disk Time", 0, &m_disk_counter) != ERROR_SUCCESS) any_failed = true;
            
            // Thermal and GPU are optional
            if (PdhAddCounter(m_query, "\\Thermal Zone Information(*)\\Temperature", 0, &m_thermal_counter) != ERROR_SUCCESS) {
                std::cerr << "[Sensors] Thermal counters not found. Thermal telemetry will be disabled." << std::endl;
                m_thermal_counter = nullptr;
            }

            if (PdhAddCounter(m_query, "\\GPU Engine(*)\\Utilization Percentage", 0, &m_gpu_counter) != ERROR_SUCCESS) {
                std::cerr << "[Sensors] GPU Performance counters not found. GPU telemetry will be disabled." << std::endl;
                m_gpu_counter = nullptr;
            }
            
            if (any_failed) {
                 std::cerr << "[Sensors] One or more critical performance counters failed to initialize." << std::endl;
            }

            PdhCollectQueryData(m_query); // Initial collection
        } else {
            std::cerr << "[Sensors] Failed to open PDH query" << std::endl;
        }

        std::cout << "[Sensors] Telemetry lanes initialized." << std::endl;
    }

    void SensorManager::stop() {
        m_running = false;
        if (m_hook) {
            UnhookWinEvent(m_hook);
            m_hook = nullptr;
        }
        if (m_query) {
            PdhCloseQuery(m_query);
            m_query = nullptr;
        }
    }

    void CALLBACK SensorManager::win_event_proc(
        HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd,
        LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime) {
        
        SensorManager* instance = s_instance.load();
        if (event == EVENT_SYSTEM_FOREGROUND && instance && instance->m_running) {
            int length = GetWindowTextLengthA(hwnd);
            if (length > 0) {
                std::string windowTitle(length + 1, '\0');
                GetWindowTextA(hwnd, &windowTitle[0], length + 1);
                instance->m_db.set("Foreground_App", windowTitle);
            }
        }
    }

    void SensorManager::collect_performance_metrics() {
        if (!m_query) return;

        if (PdhCollectQueryData(m_query) == ERROR_SUCCESS) {
            PDH_FMT_COUNTERVALUE cpu_val;
            PDH_FMT_COUNTERVALUE queue_val;
            PDH_FMT_COUNTERVALUE disk_val;

            if (PdhGetFormattedCounterValue(m_cpu_counter, PDH_FMT_DOUBLE, NULL, &cpu_val) == ERROR_SUCCESS) {
                m_db.set("CPU_Utilization", cpu_val.doubleValue);
            }

            if (PdhGetFormattedCounterValue(m_queue_counter, PDH_FMT_LONG, NULL, &queue_val) == ERROR_SUCCESS) {
                m_db.set("Thread_Queue_Length", (int)queue_val.longValue);
            }

            if (PdhGetFormattedCounterValue(m_disk_counter, PDH_FMT_DOUBLE, NULL, &disk_val) == ERROR_SUCCESS) {
                m_db.set("Disk_Utilization", disk_val.doubleValue);
            }

            // Fix for Critical #5: Thermal Kelvin Conversion Auto-detection
            if (m_thermal_counter) {
                DWORD dwBufferSize = 0;
                DWORD dwItemCount = 0;
                PdhGetFormattedCounterArrayA(m_thermal_counter, PDH_FMT_DOUBLE, &dwBufferSize, &dwItemCount, NULL);
                if (dwBufferSize > 0) {
                    std::vector<char> buffer(dwBufferSize);
                    if (PdhGetFormattedCounterArrayA(m_thermal_counter, PDH_FMT_DOUBLE, &dwBufferSize, &dwItemCount, (PPDH_FMT_COUNTERVALUE_ITEM_A)buffer.data()) == ERROR_SUCCESS) {
                        PPDH_FMT_COUNTERVALUE_ITEM_A items = (PPDH_FMT_COUNTERVALUE_ITEM_A)buffer.data();
                        double max_temp_c = 0;
                        for (DWORD i = 0; i < dwItemCount; i++) {
                            double raw = items[i].FmtValue.doubleValue;
                            double temp_c;
                            // Tenths-of-Kelvin: range ~2731-3731 for 0-100C
                            // Plain Kelvin: range ~273-373 for 0-100C
                            if (raw > 1000.0) {
                                temp_c = (raw / 10.0) - 273.15;
                            } else if (raw > 200.0) {
                                temp_c = raw - 273.15;
                            } else {
                                temp_c = raw; // Assume Celsius
                            }
                            max_temp_c = std::max(max_temp_c, std::clamp(temp_c, 0.0, 120.0));
                        }
                        m_db.set("Thermal_Headroom", max_temp_c);
                    }
                }
            }

            // Fix for Critical #4: GPU Utilization (Max Engine Proxy)
            if (m_gpu_counter) {
                DWORD dwBufferSize = 0;
                DWORD dwItemCount = 0;
                PdhGetFormattedCounterArrayA(m_gpu_counter, PDH_FMT_DOUBLE, &dwBufferSize, &dwItemCount, NULL);
                if (dwBufferSize > 0) {
                    std::vector<char> buffer(dwBufferSize);
                    if (PdhGetFormattedCounterArrayA(m_gpu_counter, PDH_FMT_DOUBLE, &dwBufferSize, &dwItemCount, (PPDH_FMT_COUNTERVALUE_ITEM_A)buffer.data()) == ERROR_SUCCESS) {
                        PPDH_FMT_COUNTERVALUE_ITEM_A items = (PPDH_FMT_COUNTERVALUE_ITEM_A)buffer.data();
                        double max_engine = 0;
                        for (DWORD i = 0; i < dwItemCount; i++) {
                            max_engine = std::max(max_engine, items[i].FmtValue.doubleValue);
                        }
                        m_db.set("GPU_Utilization", max_engine);
                    }
                }
            }
        }
    }
}
