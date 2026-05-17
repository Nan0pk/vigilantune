#include <iostream>
#include <windows.h>
#include <powrprof.h>

#pragma comment(lib, "PowrProf.lib")

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: watchdog.exe <PID_TO_MONITOR>" << std::endl;
        return 1;
    }

    DWORD pid = std::stoul(argv[1]);
    HANDLE hProcess = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION, FALSE, pid);

    if (hProcess == NULL) {
        std::cerr << "Failed to open process handle for PID: " << pid << std::endl;
        return 1;
    }

    std::cout << "[Watchdog] Successfully attached to PID: " << pid << ". Monitoring for crashes..." << std::endl;

    // Zero-overhead wait in the kernel
    WaitForSingleObject(hProcess, INFINITE);

    DWORD exitCode = 0;
    if (GetExitCodeProcess(hProcess, &exitCode)) {
        if (exitCode != 0) {
            std::cout << "[Watchdog] Agent CRASHED (Exit Code: " << exitCode << "). Initiating Fail-Safe..." << std::endl;
            
            // Reset to Balanced Scheme (GUID: 381b4222-f694-41f0-9685-ff5bb260df2e)
            GUID balanced_guid = { 0x381b4222, 0xf694, 0x41f0, { 0x96, 0x85, 0xff, 0x5b, 0xb2, 0x60, 0xdf, 0x2e } };
            if (PowerSetActiveScheme(NULL, &balanced_guid) == ERROR_SUCCESS) {
                std::cout << "[Watchdog] Fail-Safe: System Power Scheme reset to BALANCED." << std::endl;
            } else {
                std::cerr << "[Watchdog] Fail-Safe FAILED: Could not reset power scheme." << std::endl;
            }
        } else {
            std::cout << "[Watchdog] Agent exited cleanly. Closing watchdog." << std::endl;
        }
    }

    CloseHandle(hProcess);
    return 0;
}
