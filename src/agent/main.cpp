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

using namespace wspa;

std::atomic<bool> g_running(true);
std::mutex g_shutdown_mutex;
std::condition_variable g_shutdown_cv;

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
        sensors.collect_performance_metrics();
        
        std::unique_lock<std::mutex> lock(g_shutdown_mutex);
        g_shutdown_cv.wait_for(lock, std::chrono::milliseconds(config::TELEMETRY_INTERVAL_MS), [] { return !g_running.load(); });
    }
}

void control_loop(TagDatabase& db, ActuatorManager& actuators, Controller& controller) {
    int interval_ms = config::MAX_CONTROL_LOOP_INTERVAL_MS;
    
    while (g_running) {
        auto result = controller.evaluate(db);
        interval_ms = result.recommended_interval_ms;

        Tag cpu_tag, queue_tag;
        if (db.get("CPU_Utilization", cpu_tag) && db.get("Thread_Queue_Length", queue_tag)) {
            double cpu = std::get<double>(cpu_tag.value);
            int queue = std::get<int>(queue_tag.value);
            std::cout << "\r[Telemetry] CPU: " << std::fixed << std::setprecision(1) << cpu 
                      << "% | Queue: " << queue 
                      << " | SSS: " << std::setprecision(0) << result.stress_score 
                      << " | Interval: " << interval_ms << "ms   " << std::flush;
        }

        for (const auto& [param, value] : result.adjustments) {
            actuators.queue_adjustment(param, value);
        }
        
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

int main() {
    std::cout << "--- Windows SCADA Power Agent (WSPA) ---" << std::endl;

    static DWORD s_mainThreadId = GetCurrentThreadId();

    TagDatabase db;
    SensorManager sensors(db);
    ActuatorManager actuators;
    Controller controller;

    sensors.start();

    std::thread telemetry(telemetry_thread, std::ref(sensors));
    std::thread ai(control_loop, std::ref(db), std::ref(actuators), std::ref(controller));

    SetConsoleCtrlHandler([](DWORD type) -> BOOL {
        if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT) {
            std::cout << "\n[Main] Shutdown signal received..." << std::endl;
            g_running = false;
            {
                std::lock_guard<std::mutex> lock(g_shutdown_mutex);
                g_shutdown_cv.notify_all();
            }
            PostThreadMessage(s_mainThreadId, WM_QUIT, 0, 0);
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
    g_running = false;
    
    {
        std::lock_guard<std::mutex> lock(g_shutdown_mutex);
        g_shutdown_cv.notify_all();
    }

    if (telemetry.joinable()) telemetry.join();
    if (ai.joinable()) ai.join();

    sensors.stop();
    return 0;
}
