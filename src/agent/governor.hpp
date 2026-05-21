#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include <unordered_set>
#include <memory>

namespace nanoloop {
    class ProcessGovernor {
    public:
        ProcessGovernor();
        ~ProcessGovernor();

        // Performs a governance cycle: identifies bg processes and applies limits
        void govern();

    private:
        struct CpuTopology {
            std::vector<ULONG> p_core_ids;
            std::vector<ULONG> e_core_ids;
            bool is_hybrid = false;
        };

        void discover_topology();
        void apply_limits(HANDLE hProcess, bool is_background);
        
        CpuTopology m_topology;
        std::unordered_set<std::string> m_exclusion_list;
        std::unordered_set<DWORD> m_governed_pids;
        
        DWORD m_last_foreground_pid = 0;

        // Internal Win32 helper for process names
        std::string get_process_name(DWORD pid);
    };
} // namespace nanoloop
