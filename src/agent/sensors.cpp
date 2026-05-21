#include "sensors.hpp"
#include <vector>
#include <algorithm>
#include <powrprof.h>
#include <pdhmsg.h>
#include <thread>
#include "../shared/logger.hpp"

#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "PowrProf.lib")

namespace nanoloop {
    std::atomic<SensorManager*> SensorManager::s_instance = nullptr;

    SensorManager::SensorManager(TagDatabase& db) 
        : m_db(db), m_hook(nullptr), m_running(false), m_query(nullptr), m_active_callbacks(0) {
        s_instance.store(this);
    }

    SensorManager::~SensorManager() {
        stop();
    }

    void SensorManager::start() {
        m_running = true;
        s_instance.store(this);
        
        // 1. Interrupt Lane: Foreground Window Tracking
        m_hook.reset(SetWinEventHook(
            EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
            NULL, win_event_proc, 0, 0, WINEVENT_OUTOFCONTEXT));

        // 2. Coalesced Lane: Initialize PDH for performance metrics
        PDH_HQUERY raw_query = nullptr;
        if (PdhOpenQuery(NULL, 0, &raw_query) == ERROR_SUCCESS) {
            m_query.reset(raw_query);
            bool any_failed = false;
            if (PdhAddEnglishCounterA(m_query.get(), "\\Processor(_Total)\\% Processor Time", 0, &m_cpu_counter) != ERROR_SUCCESS) any_failed = true;
            if (PdhAddEnglishCounterA(m_query.get(), "\\System\\Processor Queue Length", 0, &m_queue_counter) != ERROR_SUCCESS) any_failed = true;
            if (PdhAddEnglishCounterA(m_query.get(), "\\PhysicalDisk(_Total)\\% Disk Time", 0, &m_disk_counter) != ERROR_SUCCESS) any_failed = true;
            
            // Thermal and GPU are optional
            if (PdhAddEnglishCounterA(m_query.get(), "\\Thermal Zone Information(*)\\Temperature", 0, &m_thermal_counter) != ERROR_SUCCESS) {
                LOG_WARN("Sensors", "Thermal counters not found. Thermal telemetry will be disabled.");
                m_thermal_counter = nullptr;
            }

            if (PdhAddEnglishCounterA(m_query.get(), "\\GPU Engine(*)\\Utilization Percentage", 0, &m_gpu_counter) != ERROR_SUCCESS) {
                LOG_WARN("Sensors", "GPU Performance counters not found. GPU telemetry will be disabled.");
                m_gpu_counter = nullptr;
            }
            
            if (any_failed) {
                 LOG_ERROR("Sensors", "One or more critical performance counters failed to initialize.");
            }

            PdhCollectQueryData(m_query.get()); // Initial collection
        } else {
            LOG_ERROR("Sensors", "Failed to open PDH query");
        }

        LOG_INFO("Sensors", "Telemetry lanes initialized.");
    }

    void SensorManager::stop() {
        m_running = false;
        s_instance.store(nullptr);

        m_hook.reset();

        // Wait for any in-flight callbacks to finish
        while (m_active_callbacks > 0) {
            std::this_thread::yield();
        }

        m_query.reset();
    }

    void CALLBACK SensorManager::win_event_proc(
        HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd,
        LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime) {
        
        SensorManager* instance = s_instance.load();
        if (!instance) return;

        instance->m_active_callbacks++;
        
        if (event == EVENT_SYSTEM_FOREGROUND && instance->m_running) {
            int length = GetWindowTextLengthA(hwnd);
            if (length > 0) {
                std::string windowTitle(length + 1, '\0');
                GetWindowTextA(hwnd, &windowTitle[0], length + 1);
                
                // Hash string immediately to maintain lock-free purely numeric TagDatabase
                uint32_t hash = 2166136261u;
                for (char c : windowTitle) {
                    if (c == '\0') break;
                    hash ^= (uint8_t)c;
                    hash *= 16777619u;
                }
                instance->m_db.set(TagID::Foreground_App_Hash, (double)hash);
            }
        }

        instance->m_active_callbacks--;
    }

    void SensorManager::collect_performance_metrics() {
        if (!m_query) return;

        if (PdhCollectQueryData(m_query.get()) == ERROR_SUCCESS) {
            PDH_FMT_COUNTERVALUE cpu_val;
            PDH_FMT_COUNTERVALUE queue_val;
            PDH_FMT_COUNTERVALUE disk_val;

            if (PdhGetFormattedCounterValue(m_cpu_counter, PDH_FMT_DOUBLE, NULL, &cpu_val) == ERROR_SUCCESS) {
                if (cpu_val.CStatus == PDH_CSTATUS_VALID_DATA || cpu_val.CStatus == PDH_CSTATUS_NEW_DATA) {
                    m_db.set(TagID::CPU_Utilization, std::clamp(cpu_val.doubleValue, 0.0, 100.0));
                }
            }

            if (PdhGetFormattedCounterValue(m_queue_counter, PDH_FMT_LONG, NULL, &queue_val) == ERROR_SUCCESS) {
                if (queue_val.CStatus == PDH_CSTATUS_VALID_DATA || queue_val.CStatus == PDH_CSTATUS_NEW_DATA) {
                    m_db.set(TagID::Thread_Queue_Length, (double)std::max(0L, queue_val.longValue));
                }
            }

            if (PdhGetFormattedCounterValue(m_disk_counter, PDH_FMT_DOUBLE, NULL, &disk_val) == ERROR_SUCCESS) {
                if (disk_val.CStatus == PDH_CSTATUS_VALID_DATA || disk_val.CStatus == PDH_CSTATUS_NEW_DATA) {
                    m_db.set(TagID::Disk_Utilization, std::clamp(disk_val.doubleValue, 0.0, 100.0));
                }
            }

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
                            if (items[i].FmtValue.CStatus != PDH_CSTATUS_VALID_DATA && items[i].FmtValue.CStatus != PDH_CSTATUS_NEW_DATA) continue;
                            
                            double raw = items[i].FmtValue.doubleValue;
                            double temp_c;
                            if (raw > 1000.0) temp_c = (raw / 10.0) - 273.15;
                            else if (raw > 200.0) temp_c = raw - 273.15;
                            else temp_c = raw;
                            
                            max_temp_c = std::max(max_temp_c, std::clamp(temp_c, 0.0, 120.0));
                        }
                        m_db.set(TagID::Thermal_Headroom, max_temp_c);
                    }
                }
            }

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
                            if (items[i].FmtValue.CStatus == PDH_CSTATUS_VALID_DATA || items[i].FmtValue.CStatus == PDH_CSTATUS_NEW_DATA) {
                                max_engine = std::max(max_engine, std::clamp(items[i].FmtValue.doubleValue, 0.0, 100.0));
                            }
                        }
                        m_db.set(TagID::GPU_Utilization, max_engine);
                    }
                }
            }
        }
    }

    typedef NTSTATUS (NTAPI *NtQueryTimerResolutionPtr)(PULONG MinimumResolution, PULONG MaximumResolution, PULONG CurrentResolution);

    void SensorManager::collect_high_fidelity_metrics() {
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        int coreCount = sysInfo.dwNumberOfProcessors;

        std::vector<PROCESSOR_POWER_INFORMATION> ppi(coreCount);
        if (CallNtPowerInformation(ProcessorInformation, nullptr, 0, ppi.data(), sizeof(PROCESSOR_POWER_INFORMATION) * coreCount) == 0) {
            double avg_mhz = 0;
            double max_limit = 0;
            for (int i = 0; i < coreCount; i++) {
                avg_mhz += ppi[i].CurrentMhz;
                max_limit = std::max(max_limit, (double)ppi[i].MhzLimit);
            }
            m_db.set(TagID::CPU_Frequency_Avg, avg_mhz / coreCount);
            m_db.set(TagID::CPU_Thermal_Limit, max_limit);
        }

        static NtQueryTimerResolutionPtr NtQueryTimerResolution = (NtQueryTimerResolutionPtr)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryTimerResolution");
        if (NtQueryTimerResolution) {
            ULONG minRes, maxRes, currentRes;
            if (NtQueryTimerResolution(&minRes, &maxRes, &currentRes) == 0) {
                m_db.set(TagID::Timer_Resolution_100ns, (double)currentRes);
                m_db.set(TagID::Timer_Pollution, (currentRes < 10000) ? 1.0 : 0.0);
            }
        }
    }
} // namespace nanoloop
