#include <iostream>
#include <windows.h>
#include <thread>
#include <chrono>
#include <iomanip>
#include "../shared/types.hpp"
#include "sensors.hpp"
#include "actuators.hpp"
#include "controller.hpp"

using namespace wspa;

void telemetry_thread(SensorManager& sensors) {
    while (true) {
        sensors.collect_performance_metrics();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

void control_loop(TagDatabase& db, ActuatorManager& actuators, Controller& controller) {
    while (true) {
        auto result = controller.evaluate(db);

        // Debug logging for refined metrics
        if (db.count("CPU_Utilization") && db.count("Thread_Queue_Length")) {
            double cpu = std::get<double>(db["CPU_Utilization"].value);
            int queue = std::get<int>(db["Thread_Queue_Length"].value);
            std::cout << "\r[Telemetry] CPU: " << std::fixed << std::setprecision(1) << cpu 
                      << "% | Queue: " << queue 
                      << " | SSS: " << std::setprecision(0) << result.stress_score << "   " << std::flush;
        }

        // Apply adjustments using the adaptive deadband logic
        for (const auto& [param, value] : result.adjustments) {
            actuators.apply_adjustment(param, value, result.stress_score);
        }

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
    telemetry.detach();

    // Start AI control loop
    std::thread ai(control_loop, std::ref(db), std::ref(actuators), std::ref(controller));
    ai.detach();

    std::cout << "[Main] System operational. Enter Win32 message loop..." << std::endl;

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    sensors.stop();
    return 0;
}
