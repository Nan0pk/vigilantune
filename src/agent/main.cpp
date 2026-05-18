#include <iostream>
#include <windows.h>
#include <thread>
#include <chrono>
#include <iomanip>
#include <atomic>
#include <variant>
#include <condition_variable>
#include <mutex>
#include <tlhelp32.h>
#include "../shared/types.hpp"
#include "../shared/config.hpp"
#include "sensors.hpp"
#include "actuators.hpp"
#include "controller.hpp"
#include "governor.hpp"

using namespace wspa;

std::atomic<bool> g_running(true);
std::atomic<bool> g_suspended(false);
std::atomic<bool> g_battery_saver(false);
std::mutex g_shutdown_mutex;
std::condition_variable g_shutdown_cv;

// Anti-Throttling: Opt-out of Windows 11 Efficiency Mode (EcoQoS)
void DisableEfficiencyMode() {
    PROCESS_POWER_THROTTLING_STATE powerThrottling = { 0 };
    powerThrottling.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
    powerThrottling.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
    powerThrottling.StateMask = 0; // Turn OFF Power Throttling

    if (!SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &powerThrottling, sizeof(powerThrottling))) {
        // Fallback or log if unsupported (older Win10)
    }
}

// Architecture #1: Mutual Monitoring - Check if Watchdog is alive
bool IsWatchdogRunning() {
    bool running = false;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);
        if (Process32First(hSnapshot, &pe32)) {
            do {
                if (std::string(pe32.szExeFile) == "watchdog.exe") {
                    running = true;
                    break;
                }
            } while (Process32Next(hSnapshot, &pe32));
        }
        CloseHandle(hSnapshot);
    }
    return running;
}

void telemetry_thread(SensorManager& sensors) {
    while (g_running) {
        if (!g_suspended && !g_battery_saver) {
            sensors.collect_performance_metrics();
            sensors.collect_high_fidelity_metrics();
        }
        
        std::unique_lock<std::mutex> lock(g_shutdown_mutex);
        g_shutdown_cv.wait_for(lock, std::chrono::milliseconds(config::TELEMETRY_INTERVAL_MS), [] { return !g_running.load(); });
    }
}

void governor_thread(ProcessGovernor& governor) {
    while (g_running) {
        if (!g_suspended && !g_battery_saver) {
            governor.govern();
        }
        
        std::unique_lock<std::mutex> lock(g_shutdown_mutex);
        g_shutdown_cv.wait_for(lock, std::chrono::milliseconds(10000), [] { return !g_running.load(); });
    }
}

void control_loop(TagDatabase& db, ActuatorManager& actuators, Controller& controller) {
    int interval_ms = config::MAX_CONTROL_LOOP_INTERVAL_MS;
    
    while (g_running) {
        if (g_suspended || g_battery_saver) {
            std::unique_lock<std::mutex> lock(g_shutdown_mutex);
            g_shutdown_cv.wait_for(lock, std::chrono::milliseconds(1000), [] { return !g_running.load() || (!g_suspended && !g_battery_saver); });
            continue;
        }

        auto result = controller.evaluate(db);
        interval_ms = result.recommended_interval_ms;
        
        // ... (rest of telemetry print logic)
        double cpu = db.get(TagID::CPU_Utilization);
        int queue = (int)db.get(TagID::Thread_Queue_Length);
        std::cout << "\r[Telemetry] CPU: " << std::fixed << std::setprecision(1) << cpu 
                  << "% | Queue: " << queue 
                  << " | SSS: " << std::setprecision(0) << result.stress_score 
                  << " | Interval: " << interval_ms << "ms   " << std::flush;

        // Apply Global Actuations
        actuators.queue_adjustments(result.adjustments);
        actuators.commit_changes(result.stress_score);

        // Architecture #1: Mutual Monitoring - Alert if watchdog is missing
        static int watchdog_check_counter = 0;
        if (++watchdog_check_counter > 50) { // Check every ~5-10 seconds
            if (!IsWatchdogRunning()) {
                std::cerr << "\n[Warning] Watchdog process not detected! System safety reduced." << std::endl;
            }
            watchdog_check_counter = 0;
        }

        std::unique_lock<std::mutex> lock(g_shutdown_mutex);
        g_shutdown_cv.wait_for(lock, std::chrono::milliseconds(interval_ms), [] { return !g_running.load(); });
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_POWERBROADCAST:
        if (wParam == PBT_APMSUSPEND) {
            std::cout << "\n[System] Suspend detected. Pausing agent..." << std::endl;
            g_suspended = true;
        }
        else if (wParam == PBT_APMRESUMEAUTOMATIC) {
            std::cout << "\n[System] Resume detected. Re-initializing..." << std::endl;
            g_suspended = false;
        }
        else if (wParam == PBT_POWERSETTINGCHANGE) {
            POWERBROADCAST_SETTING* setting = (POWERBROADCAST_SETTING*)lParam;
            if (setting->PowerSetting == GUID_POWER_SAVING_STATUS) {
                DWORD status = *(DWORD*)setting->Data;
                g_battery_saver = (status != 0);
                std::cout << "\n[System] Battery Saver: " << (g_battery_saver ? "ON (Yielding)" : "OFF") << std::endl;
            }
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int main() {
    std::cout << "--- Windows SCADA Power Agent (WSPA) ---" << std::endl;

    // 1. Initialize Anti-Throttling & Priority
    DisableEfficiencyMode();
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

    static DWORD s_mainThreadId = GetCurrentThreadId();

    TagDatabase db;
    SensorManager sensors(db);
    ActuatorManager actuators;
    Controller controller;
    ProcessGovernor governor;

    sensors.start();

    // 2. Setup Message Window for Power Events
    WNDCLASSA wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "WSPA_MessageWindow";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, NULL, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);

    HPOWERNOTIFY hBatteryNotify = RegisterPowerSettingNotification(hwnd, &GUID_POWER_SAVING_STATUS, DEVICE_NOTIFY_WINDOW_HANDLE);

    std::thread telemetry(telemetry_thread, std::ref(sensors));
    std::thread ai(control_loop, std::ref(db), std::ref(actuators), std::ref(controller));
    std::thread gov(governor_thread, std::ref(governor));

    SetConsoleCtrlHandler([](DWORD type) -> BOOL {
        if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT) {
            std::cout << "\n[Main] Shutdown signal received..." << std::endl;
            if (g_running.exchange(false)) {
                {
                    std::lock_guard<std::mutex> lock(g_shutdown_mutex);
                    g_shutdown_cv.notify_all();
                }
                PostThreadMessage(s_mainThreadId, WM_QUIT, 0, 0);
            }
            return TRUE;
        }
        return FALSE;
    }, TRUE);

    std::cout << "[Main] System operational. Enter Win32 message loop..." << std::endl;

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    std::cout << "\n[Main] Shutting down..." << std::endl;
    if (g_running.exchange(false)) {
        {
            std::lock_guard<std::mutex> lock(g_shutdown_mutex);
            g_shutdown_cv.notify_all();
        }
    }
    
    if (hBatteryNotify) UnregisterPowerSettingNotification(hBatteryNotify);
    
    if (telemetry.joinable()) telemetry.join();
    if (ai.joinable()) ai.join();
    if (gov.joinable()) gov.join();

    sensors.stop();
    return 0;
}
