#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>
#include <shellapi.h>
#include <thread>
#include <chrono>
#include <iomanip>
#include <atomic>
#include <variant>
#include <condition_variable>
#include <mutex>
#include <tlhelp32.h>
#include <algorithm>
#include "../shared/types.hpp"
#include "../shared/config.hpp"
#include "../shared/logger.hpp"
#include "../shared/heartbeat.hpp"
#include "sensors.hpp"
#include "actuators.hpp"
#include "controller.hpp"
#include "governor.hpp"

using namespace vigilantune;

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
        // Fallback if unsupported (older Win10)
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
                if (std::string(pe32.szExeFile) == "watchdog.exe" || std::string(pe32.szExeFile) == "vigilantune_watchdog.exe") {
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
        g_shutdown_cv.wait_for(lock, std::chrono::milliseconds(nanoloop::config::TELEMETRY_INTERVAL_MS), [] { return !g_running.load(); });
    }
}

void governor_thread(nanoloop::ProcessGovernor& governor) {
    while (g_running) {
        if (!g_suspended && !g_battery_saver) {
            governor.govern();
        }
        
        std::unique_lock<std::mutex> lock(g_shutdown_mutex);
        g_shutdown_cv.wait_for(lock, std::chrono::milliseconds(nanoloop::config::GOVERNOR_INTERVAL_MS), [] { return !g_running.load(); });
    }
}

void control_loop(TagDatabase& db, ActuatorManager& actuators, Controller& controller) {
    int interval_ms = nanoloop::config::MAX_CONTROL_LOOP_INTERVAL_MS;
    
    // Safety #1: Shared Memory Heartbeat IPC Writer
    nanoloop::HeartbeatWriter heartbeat;
    if (heartbeat.valid()) {
        LOG_INFO("Main", "Shared memory heartbeat writer initialized successfully.");
    } else {
        LOG_ERROR("Main", "Failed to initialize shared memory heartbeat writer.");
    }

    while (g_running) {
        if (g_suspended || g_battery_saver) {
            std::unique_lock<std::mutex> lock(g_shutdown_mutex);
            g_shutdown_cv.wait_for(lock, std::chrono::milliseconds(1000), [] { return !g_running.load() || (!g_suspended && !g_battery_saver); });
            continue;
        }

        auto result = controller.evaluate(db);
        interval_ms = result.recommended_interval_ms;
        
        double cpu = db.get(TagID::CPU_Utilization);
        int queue = (int)db.get(TagID::Thread_Queue_Length);
        
        LOG_INFO("Telemetry", "CPU: " << std::fixed << std::setprecision(1) << cpu 
                  << "% | Queue: " << queue 
                  << " | SSS: " << std::setprecision(0) << result.stress_score 
                  << " | Interval: " << interval_ms << "ms");

        // Apply Global Actuations
        actuators.queue_adjustments(result.adjustments);
        actuators.commit_changes(result.stress_score);

        // Tick the heartbeat so the watchdog knows we're alive and responsive
        if (heartbeat.valid()) {
            heartbeat.tick();
        }

        static int watchdog_check_counter = 0;
        if (++watchdog_check_counter > 50) { // Check every ~5-10 seconds
            if (!IsWatchdogRunning()) {
                LOG_WARN("Main", "Watchdog process not detected! System safety reduced.");
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
            LOG_INFO("System", "Suspend detected. Pausing agent...");
            g_suspended = true;
        }
        else if (wParam == PBT_APMRESUMEAUTOMATIC) {
            LOG_INFO("System", "Resume detected. Re-initializing...");
            g_suspended = false;
        }
        else if (wParam == PBT_POWERSETTINGCHANGE) {
            POWERBROADCAST_SETTING* setting = (POWERBROADCAST_SETTING*)lParam;
            if (setting->PowerSetting == GUID_POWER_SAVING_STATUS) {
                DWORD status = *(DWORD*)setting->Data;
                g_battery_saver = (status != 0);
                LOG_INFO("System", "Battery Saver: " << (g_battery_saver ? "ON (Yielding)" : "OFF"));
            }
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
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

bool IsRunAsAdmin() {
    BOOL fRet = FALSE;
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD cbSize = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &cbSize)) {
            fRet = elevation.TokenIsElevated;
        }
    }
    if (hToken) {
        CloseHandle(hToken);
    }
    return fRet;
}

void EnsureAdminElevation() {
    if (!IsRunAsAdmin()) {
        char szPath[MAX_PATH];
        if (GetModuleFileNameA(NULL, szPath, ARRAYSIZE(szPath))) {
            SHELLEXECUTEINFOA sei = { sizeof(sei) };
            sei.cbSize = sizeof(sei);
            sei.fMask = SEE_MASK_NOCLOSEPROCESS;
            sei.lpVerb = "runas";
            sei.lpFile = szPath;
            sei.hwnd = NULL;
            sei.nShow = SW_NORMAL;
            if (ShellExecuteExA(&sei)) {
                ExitProcess(0);
            }
        }
    }
}

int main() {
    // 1. Enforce Administrator Self-Elevation on launch
    EnsureAdminElevation();

    // 2. Initialize Anti-Throttling & Priority
    DisableEfficiencyMode();
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

    std::string exe_dir = get_executable_directory();
    std::string config_path = exe_dir.empty() ? "nanoloop_config.ini" : (exe_dir + "\\nanoloop_config.ini");
    nanoloop::config::CONFIG_FILE_PATH = config_path;

    std::string log_file_path = exe_dir.empty() ? "nanoloop_agent.log" : (exe_dir + "\\nanoloop_agent.log");
    nanoloop::log::LoggerState::instance().initFile(log_file_path);

    LOG_INFO("Main", "--- VigilanTune Power Agent ---");

    if (nanoloop::config::load_from_file(config_path)) {
        LOG_INFO("Main", "Configuration loaded from: " << config_path);
    } else {
        LOG_WARN("Main", "Configuration file not found or invalid at: " << config_path << ". Using defaults.");
    }

    std::string log_level = "INFO";
    nanoloop::ConfigLoader loader;
    if (loader.load(config_path)) {
        log_level = loader.get_string("general.log_level", "INFO");
    }
    nanoloop::log::Level level = nanoloop::log::Level::INFO;
    std::transform(log_level.begin(), log_level.end(), log_level.begin(), ::toupper);
    if (log_level == "TRACE") level = nanoloop::log::Level::TRACE;
    else if (log_level == "DEBUG") level = nanoloop::log::Level::DEBUG;
    else if (log_level == "INFO") level = nanoloop::log::Level::INFO;
    else if (log_level == "WARN") level = nanoloop::log::Level::WARN;
    else if (log_level == "ERROR") level = nanoloop::log::Level::ERROR_LVL;
    else if (log_level == "FATAL") level = nanoloop::log::Level::FATAL;
    nanoloop::log::LoggerState::instance().setLevel(level);

    static DWORD s_mainThreadId = GetCurrentThreadId();

    TagDatabase db;
    SensorManager sensors(db);
    ActuatorManager actuators;
    Controller controller;
    nanoloop::ProcessGovernor governor;

    sensors.start();

    WNDCLASSA wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "VigilanTune_MessageWindow";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, NULL, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);

    HPOWERNOTIFY hBatteryNotify = RegisterPowerSettingNotification(hwnd, &GUID_POWER_SAVING_STATUS, DEVICE_NOTIFY_WINDOW_HANDLE);

    std::thread telemetry(telemetry_thread, std::ref(sensors));
    std::thread ai(control_loop, std::ref(db), std::ref(actuators), std::ref(controller));
    std::thread gov(governor_thread, std::ref(governor));

    SetConsoleCtrlHandler([](DWORD type) -> BOOL {
        if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT) {
            LOG_INFO("Main", "Shutdown signal received...");
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

    LOG_INFO("Main", "System operational. Enter Win32 message loop...");

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    LOG_INFO("Main", "Shutting down...");
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
    LOG_INFO("Main", "Graceful shutdown complete.");
    return 0;
}
