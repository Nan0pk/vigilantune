#include <windows.h>
#include <powrprof.h>
#include <string>
#include <vector>
#include <cmath>
#include <thread>
#include <atomic>
#include <algorithm>
#include "../shared/config.hpp"
#include "../shared/logger.hpp"
#include "../shared/scoped_handle.hpp"
#include "../shared/heartbeat.hpp"

#pragma comment(lib, "PowrProf.lib")

using namespace wspa;

bool StartAgent(const std::string& path, PROCESS_INFORMATION& pi) {
    STARTUPINFOA si = { sizeof(si) };
    if (CreateProcessA(path.c_str(), NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        return true;
    }
    return false;
}

std::string get_executable_directory() {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string exe_path(path);
    auto pos = exe_path.find_last_of("\\/");
    if (pos != std::string::npos) {
        return exe_path.substr(0, pos);
    }
    return "";
}

void monitor_heartbeat(HANDLE hProcess, DWORD pid, std::atomic<bool>& active) {
    HeartbeatReader reader;
    // Wait up to 3 seconds for the agent's heartbeat section to initialize
    for (int i = 0; i < 30; ++i) {
        if (reader.valid()) break;
        Sleep(100);
    }

    if (!reader.valid()) {
        LOG_WARN("Watchdog", "Failed to open shared memory heartbeat reader. Heartbeat monitoring disabled.");
        return;
    }

    LOG_INFO("Watchdog", "Heartbeat monitoring started for Agent PID " << pid);
    
    while (active) {
        // Check if process has already exited
        DWORD exitCode = 0;
        if (GetExitCodeProcess(hProcess, &exitCode) && exitCode != STILL_ACTIVE) {
            break;
        }

        // Verify if agent's heartbeat is alive (timeout of 5 seconds)
        if (!reader.is_alive(5000)) {
            LOG_ERROR("Watchdog", "Agent HANG detected (Heartbeat stale)! Terminating agent process...");
            TerminateProcess(hProcess, 0xDEAD); // Force crash exit code
            break;
        }

        Sleep(1000);
    }
    LOG_INFO("Watchdog", "Heartbeat monitoring stopped.");
}

int main(int argc, char* argv[]) {
    // Setup mirror file logging next to watchdog executable
    std::string exe_dir = get_executable_directory();
    std::string log_file_path = exe_dir.empty() ? "wspa_watchdog.log" : (exe_dir + "\\wspa_watchdog.log");
    log::LoggerState::instance().initFile(log_file_path);

    LOG_INFO("Watchdog", "--- Windows SCADA Watchdog Monitor ---");

    if (argc < 2) {
        LOG_FATAL("Watchdog", "Usage: watchdog.exe <AGENT_EXE_PATH>");
        return 1;
    }

    std::string agentPath = argv[1];

    // Load configuration to get max recoveries
    std::string config_path = exe_dir.empty() ? "wspa_config.ini" : (exe_dir + "\\wspa_config.ini");
    config::load_from_file(config_path);

    int recoveryCount = 0;
    int maxRecoveries = config::MAX_RECOVERY_ATTEMPTS;

    // Load and apply watchdog logger level
    std::string log_level = "INFO";
    ConfigLoader loader;
    if (loader.load(config_path)) {
        log_level = loader.get_string("general.log_level", "INFO");
    }
    log::Level level = log::Level::INFO;
    std::transform(log_level.begin(), log_level.end(), log_level.begin(), ::toupper);
    if (log_level == "TRACE") level = log::Level::TRACE;
    else if (log_level == "DEBUG") level = log::Level::DEBUG;
    else if (log_level == "INFO") level = log::Level::INFO;
    else if (log_level == "WARN") level = log::Level::WARN;
    else if (log_level == "ERROR") level = log::Level::ERROR_LVL;
    else if (log_level == "FATAL") level = log::Level::FATAL;
    log::LoggerState::instance().setLevel(level);

    while (recoveryCount <= maxRecoveries) {
        PROCESS_INFORMATION pi = { 0 };
        if (!StartAgent(agentPath, pi)) {
            LOG_ERROR("Watchdog", "Failed to start agent at: " << agentPath << " (Error: " << GetLastError() << ")");
            return 1;
        }

        ScopedHandle hProcess(pi.hProcess);
        ScopedHandle hThread(pi.hThread);

        LOG_INFO("Watchdog", "Agent started (PID: " << pi.dwProcessId << "). Monitoring...");
        
        std::atomic<bool> monitor_active(true);
        std::thread heartbeat_thread(monitor_heartbeat, hProcess.get(), pi.dwProcessId, std::ref(monitor_active));

        WaitForSingleObject(hProcess.get(), INFINITE);

        // Turn off heartbeat thread
        monitor_active = false;
        if (heartbeat_thread.joinable()) {
            heartbeat_thread.join();
        }

        DWORD exitCode = 0;
        GetExitCodeProcess(hProcess.get(), &exitCode);

        if (exitCode == 0) {
            LOG_INFO("Watchdog", "Agent exited cleanly. Exiting watchdog.");
            return 0;
        }

        recoveryCount++;
        
        LOG_ERROR("Watchdog", "Agent CRASHED/HANG (Exit Code: 0x" << std::hex << exitCode << std::dec 
                  << "). Recovery attempt " << recoveryCount << "/" << maxRecoveries << "...");
        
        // Immediate Fail-Safe Assertion: Reset system power scheme to Balanced
        if (PowerSetActiveScheme(NULL, &config::FAILSAFE_SCHEME_GUID) == ERROR_SUCCESS) {
            LOG_INFO("Watchdog", "Fail-Safe: Power Scheme reset to known-safe baseline.");
        } else {
            LOG_ERROR("Watchdog", "CRITICAL: Failed to assert fail-safe power scheme!");
        }

        if (recoveryCount > maxRecoveries) {
            LOG_FATAL("Watchdog", "Max recovery attempts reached. Terminating watchdog to prevent cyclic system instability.");
            return 1;
        }

        // Exponential Backoff cooldown
        int wait_seconds = (int)std::pow(2, recoveryCount); 
        LOG_INFO("Watchdog", "Cooling down for " << wait_seconds << " seconds before restart...");
        Sleep(wait_seconds * 1000);
    }

    return 0;
}
