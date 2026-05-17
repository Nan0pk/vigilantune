#include <iostream>
#include <windows.h>
#include <thread>
#include <chrono>
#include <iomanip>
#include <atomic>
#include <variant>
#include <condition_variable>
#include <mutex>
#include "../shared/types.hpp"
#include "sensors.hpp"
#include "actuators.hpp"
#include "controller.hpp"

using namespace wspa;

std::atomic<bool> g_running(true);
std::mutex g_shutdown_mutex;
std::condition_variable g_shutdown_cv;

void telemetry_thread(SensorManager& sensors) {
    while (g_running) {
        sensors.collect_performance_metrics();
        
        std::unique_lock<std::mutex> lock(g_shutdown_mutex);
        g_shutdown_cv.wait_for(lock, std::chrono::milliseconds(1000), [] { return !g_running.load(); });
    }
}

void control_loop(TagDatabase& db, ActuatorManager& actuators, Controller& controller) {
    while (g_running) {
        auto result = controller.evaluate(db);

        // Debug logging for refined metrics
        Tag cpu_tag, queue_tag;
        if (db.get("CPU_Utilization", cpu_tag) && db.get("Thread_Queue_Length", queue_tag)) {
            double cpu = std::get<double>(cpu_tag.value);
            int queue = std::get<int>(queue_tag.value);
            std::cout << "\r[Telemetry] CPU: " << std::fixed << std::setprecision(1) << cpu 
                      << "% | Queue: " << queue 
                      << " | SSS: " << std::setprecision(0) << result.stress_score << "   " << std::flush;
        }

        for (const auto& [param, value] : result.adjustments) {
            actuators.queue_adjustment(param, value);
        }
        
        actuators.commit_changes(result.stress_score);

        std::unique_lock<std::mutex> lock(g_shutdown_mutex);
        g_shutdown_cv.wait_for(lock, std::chrono::milliseconds(100), [] { return !g_running.load(); });
    }
}

int main() {
    std::cout << "--- Windows SCADA Power Agent (WSPA) ---" << std::endl;

    DWORD mainThreadId = GetCurrentThreadId();

    TagDatabase db;
    SensorManager sensors(db);
    ActuatorManager actuators;
    Controller controller;

    sensors.start();

    // Start Telemetry collection (Coalesced Lane)
    std::thread telemetry(telemetry_thread, std::ref(sensors));

    // Start AI control loop
    std::thread ai(control_loop, std::ref(db), std::ref(actuators), std::ref(controller));

    // Set console control handler for graceful shutdown
    // Fix for Critical #1: PostThreadMessage to ensure WM_QUIT reaches the main thread
    SetConsoleCtrlHandler([](DWORD type) -> BOOL {
        if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT) {
            std::cout << "\n[Main] Shutdown signal received..." << std::endl;
            g_running = false;
            
            // Wake up threads waiting on CV
            {
                std::lock_guard<std::mutex> lock(g_shutdown_mutex);
                g_shutdown_cv.notify_all();
            }

            // Get main thread ID passed via a mechanism or use a global
            // Since we can't easily capture in a raw function pointer, we assume global access or 
            // post to a known thread. For this prototype, we'll find the main thread.
            // (In a real app, this would be a static member or global)
            return TRUE;
        }
        return FALSE;
    }, TRUE);

    // We need the main thread ID in the handler. Let's use a static/global for simplicity in this prototype.
    static DWORD s_mainThreadId = mainThreadId;
    SetConsoleCtrlHandler([](DWORD type) -> BOOL {
        if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT) {
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
