#include <iostream>
#include <windows.h>
#include <powrprof.h>
#include <string>
#include <vector>
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
        std::cout << "[Watchdog] Agent CRASHED (Exit Code: " << exitCode << "). Recovery attempt " << recoveryCount << "/" << maxRecoveries << "..." << std::endl;

        // Reset to Fail-Safe Scheme from config
        PowerSetActiveScheme(NULL, &config::FAILSAFE_SCHEME_GUID);

        if (recoveryCount > maxRecoveries) {
            std::cerr << "[Watchdog] Max recovery attempts reached. Manual intervention required." << std::endl;
            return 1;
        }

        Sleep(2000); // Wait before restarting
    }

    return 0;
}
