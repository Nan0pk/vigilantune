#include "sensors.hpp"
#include <vector>
#include <algorithm>
#include <powrprof.h>
#include <pdhmsg.h>
#include <winioctl.h>
#include <iphlpapi.h>
#include <thread>
#include "../shared/logger.hpp"

#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "PowrProf.lib")
#pragma comment(lib, "iphlpapi.lib")

namespace vigilantune {
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

        // 3. Start Auxiliary Telemetry Lane Thread
        m_aux_thread = std::thread(&SensorManager::aux_telemetry_thread_proc, this);

        LOG_INFO("Sensors", "Telemetry lanes initialized.");
    }

    void SensorManager::stop() {
        m_running = false;
        s_instance.store(nullptr);

        m_hook.reset();

        // Join auxiliary thread
        if (m_aux_thread.joinable()) {
            m_aux_thread.join();
        }

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

    // --- AUXILIARY TELEMETRY LANE IMPLEMENTATION (Decoupled every 5 seconds) ---
    
    void SensorManager::aux_telemetry_thread_proc() {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        while (m_running) {
            collect_memory_metrics();
            collect_battery_metrics();
            
            double disk_temp = get_primary_drive_temperature();
            if (disk_temp > 0.0) {
                // Disk temp can be logged or added to SSS if needed
            }

            double net_throughput = get_network_throughput();
            m_db.set(TagID::Network_Throughput, net_throughput);

            // ── Shared-Heatpipe Thermal Extrapolation Model ──
            // On laptops, CPU/GPU/SSD often share the same heatpipe and fan assembly.
            // If a direct temperature sensor is unavailable for one component, we
            // extrapolate from whichever sensor IS reporting, because in a shared
            // thermal solution all components trend together.

            double cpu_temp = m_db.get(TagID::Thermal_Headroom);
            GpuMetrics gpu_metrics = m_gpu_loader.get_metrics();
            double gpu_temp = 0.0;

            if (gpu_metrics.is_valid && gpu_metrics.temperature_c > 0.0) {
                // ── Path A: Direct GPU temp available (NVML / ADL) ──
                gpu_temp = gpu_metrics.temperature_c;
                m_db.set(TagID::GPU_Temperature, gpu_temp);
            } else if (cpu_temp > 0.0) {
                // ── Path B: Only CPU temp available → extrapolate GPU ≈ CPU ──
                // Shared heatpipe: GPU die is typically within ±5°C of CPU on laptops
                gpu_temp = cpu_temp;
                m_db.set(TagID::GPU_Temperature, std::clamp(gpu_temp, 0.0, 120.0));
                LOG_DEBUG("Sensors", "GPU temp extrapolated from CPU temp: " << gpu_temp << "°C");
            } else if (disk_temp > 0.0) {
                // ── Path C: Only SSD/HDD temp available → assume system is hot ──
                // If the drive is warm, the chassis is warm — assume CPU & GPU similar
                gpu_temp = disk_temp;
                m_db.set(TagID::GPU_Temperature, std::clamp(gpu_temp, 0.0, 120.0));
                LOG_DEBUG("Sensors", "GPU temp extrapolated from disk temp: " << gpu_temp << "°C");
            } else {
                // ── Path D: No thermal data at all — use safe assumption ──
                m_db.set(TagID::GPU_Temperature, 0.0);
            }

            // ── Cross-Extrapolation: fill missing CPU temp from GPU or disk ──
            if (cpu_temp <= 0.0) {
                double best_known = std::max(gpu_temp, disk_temp);
                if (best_known > 0.0) {
                    m_db.set(TagID::Thermal_Headroom, std::clamp(best_known, 0.0, 120.0));
                    LOG_DEBUG("Sensors", "CPU temp extrapolated from best known sensor: " << best_known << "°C");
                }
            } else if (disk_temp > cpu_temp && disk_temp > gpu_temp && disk_temp > 50.0) {
                // SSD running hotter than both CPU and GPU readings — chassis is hot,
                // bump the system thermal baseline up to the disk temp
                m_db.set(TagID::Thermal_Headroom, std::clamp(disk_temp, 0.0, 120.0));
                LOG_DEBUG("Sensors", "System thermal baseline raised to disk temp: " << disk_temp << "°C");
            }

            for (int i = 0; i < 50 && m_running; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }

    double SensorManager::get_primary_drive_temperature() {
        HANDLE hDevice = CreateFileA("\\\\.\\PhysicalDrive0", 
            GENERIC_READ | GENERIC_WRITE, 
            FILE_SHARE_READ | FILE_SHARE_WRITE, 
            NULL, OPEN_EXISTING, 0, NULL);
        if (hDevice == INVALID_HANDLE_VALUE) return 0.0;

        STORAGE_PROPERTY_QUERY query = {};
        query.PropertyId = StorageDeviceTemperatureProperty;
        query.QueryType = PropertyStandardQuery;

        BYTE buffer[512] = {};
        DWORD bytesReturned = 0;

        if (DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY,
            &query, sizeof(query),
            buffer, sizeof(buffer),
            &bytesReturned, NULL)) {
            
            struct TEMP_DESCRIPTOR {
                ULONG Version;
                ULONG Size;
                SHORT Temperature;
                SHORT OverThreshold;
                SHORT UnderThreshold;
            };
            
            auto* desc = (TEMP_DESCRIPTOR*)buffer;
            if (desc->Size > 0 && desc->Temperature != 0) {
                CloseHandle(hDevice);
                return (double)desc->Temperature;
            }
        }
        CloseHandle(hDevice);
        return 0.0;
    }

    double SensorManager::get_network_throughput() {
        PMIB_IF_TABLE2 pIfTable = NULL;
        double total_throughput = 0.0;
        if (GetIfTable2(&pIfTable) == NO_ERROR) {
            static ULONGLONG last_bytes = 0;
            static ULONGLONG last_time = 0;
            
            ULONGLONG current_bytes = 0;
            for (ULONG i = 0; i < pIfTable->NumEntries; i++) {
                if (pIfTable->Table[i].Type == IF_TYPE_SOFTWARE_LOOPBACK) continue;
                current_bytes += pIfTable->Table[i].InOctets + pIfTable->Table[i].OutOctets;
            }

            ULONGLONG now = GetTickCount64();
            if (last_time > 0 && now > last_time) {
                double sec = (double)(now - last_time) / 1000.0;
                total_throughput = (double)(current_bytes - last_bytes) / sec;
            }
            last_bytes = current_bytes;
            last_time = now;
            FreeMibTable(pIfTable);
        }
        return total_throughput;
    }

    void SensorManager::collect_battery_metrics() {
        SYSTEM_POWER_STATUS status;
        if (GetSystemPowerStatus(&status)) {
            m_db.set(TagID::Battery_Percent, status.BatteryLifePercent == 255 ? 100.0 : (double)status.BatteryLifePercent);
        }

        SYSTEM_BATTERY_STATE batteryState = {};
        if (CallNtPowerInformation(SystemBatteryState, NULL, 0, &batteryState, sizeof(batteryState)) == 0) {
            m_db.set(TagID::Battery_Power_Rate, (double)batteryState.Rate);
        }
    }

    void SensorManager::collect_memory_metrics() {
        MEMORYSTATUSEX memInfo = {};
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        if (GlobalMemoryStatusEx(&memInfo)) {
            m_db.set(TagID::Memory_Utilization, (double)memInfo.dwMemoryLoad);
        }
    }
} // namespace vigilantune
