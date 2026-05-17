#include <iostream>
#include <windows.h>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: watchdog.exe <PID_TO_MONITOR>" << std::endl;
        return 1;
    }

    DWORD pid = std::stoul(argv[1]);
    HANDLE hProcess = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION, FALSE, pid);

    if (hProcess == NULL) {
        std::cerr << "Failed to open process handle." << std::endl;
        return 1;
    }

    std::cout << "[Watchdog] Monitoring PID: " << pid << std::endl;

    // Zero-overhead wait
    WaitForSingleObject(hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(hProcess, &exitCode);

    if (exitCode != 0) {
        std::cout << "[Watchdog] Agent crashed with exit code " << exitCode << ". Resetting power plan..." << std::endl;
        // system("powercfg /setactive SCHEME_BALANCED");
    } else {
        std::cout << "[Watchdog] Agent exited cleanly." << std::endl;
    }

    CloseHandle(hProcess);
    return 0;
}
