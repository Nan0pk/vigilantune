#include <iostream>
#include <windows.h>
#include <thread>
#include <chrono>
#include <iomanip>
#include <atomic>
#include <variant>
#include "../shared/types.hpp"
#include "sensors.hpp"
#include "actuators.hpp"
#include "controller.hpp"

using namespace wspa;

std::atomic<bool> g_running(true);

void telemetry_thread(SensorManager& sensors) {
    while (g_running) {
        sensors.collect_performance_metrics();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
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

        // Queue adjustments (don't apply yet)
        for (const auto& [param, value] : result.adjustments) {
            actuators.queue_adjustment(param, value);
        }
        
        // Commit all queued changes in a single batch
        actuators.commit_changes(result.stress_score);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int main() {
    std::cout << "--- Windows SCADA Power Agent (WSPA) ---" << std::endl;

    TagDatabase db;
    SensorManager sensors(db);
    ActuatorManager actuators;
    Controller controller;

    sensors.start();

    // Start Telemetry collection (Coalesced Lane)
    std::thread telemetry(telemetry_thread, std::ref(sensors));

    // Start AI control loop
    std::thread ai(control_loop, std::ref(db), std::ref(actuators), std::ref(controller));

    std::cout << "[Main] System operational. Enter Win32 message loop..." << std::endl;

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message == WM_QUIT) break;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    std::cout << "\n[Main] Shutting down..." << std::endl;
    g_running = false;

    if (telemetry.joinable()) telemetry.join();
    if (ai.joinable()) ai.join();

    sensors.stop();
    return 0;
}
