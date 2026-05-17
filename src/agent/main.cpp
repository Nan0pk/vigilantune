#include <iostream>
#include <windows.h>
#include <thread>
#include <chrono>
#include "../shared/types.hpp"
#include "sensors.hpp"
#include "actuators.hpp"
#include "controller.hpp"

using namespace wspa;

void control_loop(TagDatabase& db, ActuatorManager& actuators, Controller& controller) {
    while (true) {
        // Evaluate system state
        auto adjustments = controller.evaluate(db);

        // Apply adjustments if any
        for (const auto& [param, value] : adjustments) {
            std::cout << "[Agent] Applying adjustment: " << param << " -> " << value << std::endl;
            // actuators.update_setting(...)
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int main() {
    std::cout << "Windows SCADA Power Agent (WSPA) Starting..." << std::endl;

    TagDatabase db;
    SensorManager sensors(db);
    ActuatorManager actuators;
    Controller controller;

    sensors.start();

    // Start AI control loop in a separate thread
    std::thread ai_thread(control_loop, std::ref(db), std::ref(actuators), std::ref(controller));
    ai_thread.detach();

    // Standard Win32 Message Loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    sensors.stop();
    return 0;
}
