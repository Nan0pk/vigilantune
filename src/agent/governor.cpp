#include "governor.hpp"
#include <psapi.h>
#include <tlhelp32.h>
#include <algorithm>
#include "../shared/scoped_handle.hpp"
#include "../shared/logger.hpp"
#include "../shared/config.hpp"

namespace nanoloop {

    ProcessGovernor::ProcessGovernor() {
        discover_topology();
        
        // Load exclusion list from configuration
        for (const auto& name : config::EXCLUSION_LIST) {
            std::string lower_name = name;
            std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
            m_exclusion_list.insert(lower_name);
        }
        
        LOG_INFO("Governor", "Loaded " << m_exclusion_list.size() << " process exclusions from configuration.");
    }

    ProcessGovernor::~ProcessGovernor() {}

    void ProcessGovernor::discover_topology() {
        ULONG bufSize = 0;
        if (!GetSystemCpuSetInformation(nullptr, 0, &bufSize, GetCurrentProcess(), 0) && GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
            auto buf = std::make_unique<uint8_t[]>(bufSize);
            if (GetSystemCpuSetInformation((PSYSTEM_CPU_SET_INFORMATION)buf.get(), bufSize, &bufSize, GetCurrentProcess(), 0)) {
                
                uint8_t* ptr = buf.get();
                while (ptr < buf.get() + bufSize) {
                    PSYSTEM_CPU_SET_INFORMATION info = (PSYSTEM_CPU_SET_INFORMATION)ptr;
                    if (info->Type == CpuSetInformation) {
                        // EfficiencyClass: Higher value usually means more performant (P-core)
                        // This varies by vendor, but 0 is generally "Efficient"
                        if (info->CpuSet.EfficiencyClass > 0) {
                            m_topology.p_core_ids.push_back(info->CpuSet.Id);
                        } else {
                            m_topology.e_core_ids.push_back(info->CpuSet.Id);
                        }
                    }
                    ptr += info->Size;
                }
            }
        }

        m_topology.is_hybrid = !m_topology.e_core_ids.empty() && !m_topology.p_core_ids.empty();
        
        if (m_topology.is_hybrid) {
            LOG_INFO("Governor", "Hybrid Topology Detected: " 
                      << m_topology.p_core_ids.size() << " P-cores, " 
                      << m_topology.e_core_ids.size() << " E-cores.");
        } else {
            LOG_INFO("Governor", "Symmetric Topology Detected (or API unsupported).");
        }
    }

    void ProcessGovernor::govern() {
        HWND foreground_hwnd = GetForegroundWindow();
        DWORD foreground_pid = 0;
        GetWindowThreadProcessId(foreground_hwnd, &foreground_pid);

        bool foreground_changed = (foreground_pid != m_last_foreground_pid);
        m_last_foreground_pid = foreground_pid;

        // In the dedicated thread, every loop iteration is a scan, so we clear the cache
        m_governed_pids.clear(); 

        ScopedHandle hSnapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
        if (!hSnapshot) return;

        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);

        if (Process32First(hSnapshot.get(), &pe32)) {
            do {
                if (pe32.th32ProcessID == 0) continue; // Idle process

                if (pe32.th32ProcessID == foreground_pid) {
                    if (foreground_changed) {
                        ScopedHandle hProcess(OpenProcess(PROCESS_SET_INFORMATION, FALSE, pe32.th32ProcessID));
                        if (hProcess) {
                            // 1. Ensure foreground has P-cores if hybrid
                            if (m_topology.is_hybrid) {
                                SetProcessDefaultCpuSets(hProcess.get(), m_topology.p_core_ids.data(), (ULONG)m_topology.p_core_ids.size());
                            }

                            // 2. Disable EcoQoS (Turn OFF Efficiency Mode)
                            PROCESS_POWER_THROTTLING_STATE pts = { 0 };
                            pts.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
                            pts.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
                            pts.StateMask = 0; // Turn OFF execution speed throttling
                            SetProcessInformation(hProcess.get(), ProcessPowerThrottling, &pts, sizeof(pts));

                            // 3. Restore Memory Priority (Normal)
                            MEMORY_PRIORITY_INFORMATION mpi = { MEMORY_PRIORITY_NORMAL };
                            SetProcessInformation(hProcess.get(), ProcessMemoryPriority, &mpi, sizeof(mpi));

                            // 4. Restore Priority Class (Normal)
                            SetPriorityClass(hProcess.get(), NORMAL_PRIORITY_CLASS);

                            LOG_DEBUG("Governor", "Restored foreground application performance (PID: " << pe32.th32ProcessID << ").");
                        }
                    }
                    continue;
                }

                // If already governed in this scan window, skip
                if (m_governed_pids.count(pe32.th32ProcessID)) continue;

                std::string name = pe32.szExeFile;
                std::transform(name.begin(), name.end(), name.begin(), ::tolower);

                if (m_exclusion_list.count(name)) continue;

                // Identify potential background noise
                ScopedHandle hProcess(OpenProcess(PROCESS_SET_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe32.th32ProcessID));
                if (hProcess) {
                    apply_limits(hProcess.get(), true);
                    m_governed_pids.insert(pe32.th32ProcessID);
                }

            } while (Process32Next(hSnapshot.get(), &pe32));
        }
    }

    void ProcessGovernor::apply_limits(HANDLE hProcess, bool is_background) {
        if (!is_background) return;

        // 1. EcoQoS (Efficiency Mode)
        PROCESS_POWER_THROTTLING_STATE pts = { 0 };
        pts.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
        pts.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
        pts.StateMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED; 
        SetProcessInformation(hProcess, ProcessPowerThrottling, &pts, sizeof(pts));

        // 2. Memory Priority (Low)
        MEMORY_PRIORITY_INFORMATION mpi = { MEMORY_PRIORITY_LOWEST };
        SetProcessInformation(hProcess, ProcessMemoryPriority, &mpi, sizeof(mpi));

        // 3. CPU Sets (Move to E-cores)
        if (m_topology.is_hybrid && !m_topology.e_core_ids.empty()) {
            SetProcessDefaultCpuSets(hProcess, m_topology.e_core_ids.data(), (ULONG)m_topology.e_core_ids.size());
        }
        
        // 4. Background Priority Class
        // Note: PROCESS_MODE_BACKGROUND_BEGIN is only supported on the current process (GetCurrentProcess()).
        // For external processes, we use IDLE_PRIORITY_CLASS.
        SetPriorityClass(hProcess, IDLE_PRIORITY_CLASS);
    }

    std::string ProcessGovernor::get_process_name(DWORD pid) {
        char buffer[MAX_PATH];
        ScopedHandle hProcess(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
        if (hProcess) {
            if (GetModuleBaseNameA(hProcess.get(), NULL, buffer, MAX_PATH)) {
                return std::string(buffer);
            }
        }
        return "";
    }
} // namespace nanoloop
