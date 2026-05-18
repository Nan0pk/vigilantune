#include <iostream>
#include <windows.h>
#include <powrprof.h>
#include <string>
#include <vector>
#include <cmath>
#include "../shared/config.hpp"

#pragma comment(lib, "PowrProf.lib")

using namespace wspa;

bool StartAgent(const std::string& path, PROCESS_INFORMATION& pi) {
    STARTUPINFOA si = { sizeof(si) };
    if (CreateProcessA(path.c_str(), NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        return true;
    }
    return false;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: watchdog.exe <AGENT_EXE_PATH>" << std::endl;
        return 1;
    }

    std::string agentPath = argv[1];
    int recoveryCount = 0;
    const int maxRecoveries = 3;

    // Fix for Critical #3: Watchdog loop off-by-one
    while (recoveryCount <= maxRecoveries) {
        PROCESS_INFORMATION pi = { 0 };
        if (!StartAgent(agentPath, pi)) {
            std::cerr << "[Watchdog] Failed to start agent at: " << agentPath << " (Error: " << GetLastError() << ")" << std::endl;
            return 1;
        }

        std::cout << "[Watchdog] Agent started (PID: " << pi.dwProcessId << "). Monitoring..." << std::endl;
        
        WaitForSingleObject(pi.hProcess, INFINITE);

        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        if (exitCode == 0) {
            std::cout << "[Watchdog] Agent exited cleanly. Exiting watchdog." << std::endl;
            return 0;
        }

        recoveryCount++;
        
        // Security #3: Configurable Safe State
        std::cout << "[Watchdog] Agent CRASHED (Exit Code: " << exitCode << "). Recovery attempt " << recoveryCount << "/" << maxRecoveries << "..." << std::endl;
        if (PowerSetActiveScheme(NULL, &config::FAILSAFE_SCHEME_GUID) == ERROR_SUCCESS) {
            std::cout << "[Watchdog] Fail-Safe: Power Scheme reset to configured default." << std::endl;
        }

        if (recoveryCount > maxRecoveries) {
            std::cerr << "[Watchdog] Max recovery attempts reached. Manual intervention required." << std::endl;
            return 1;
        }

        // Monitoring Loop for Power Scheme Overrides
        std::cout << "[Watchdog] Starting interference monitoring..." << std::endl;
        for (int i = 0; i < 5; ++i) { // Simple poll loop before restart
            GUID* active_guid = nullptr;
            if (PowerGetActiveScheme(NULL, &active_guid) == ERROR_SUCCESS) {
                // If the active scheme is not a known safe one, we could force it here
                // For now, just log the interference
                LocalFree(active_guid);
            }
            Sleep(1000);
        }

        // Implementation #3: Exponential Backoff (Wait longer after each crash)
        int wait_seconds = (int)std::pow(2, recoveryCount); 
        std::cout << "[Watchdog] Waiting " << wait_seconds << " seconds before restart..." << std::endl;
        Sleep(wait_seconds * 1000);
    }

    return 0;
}
