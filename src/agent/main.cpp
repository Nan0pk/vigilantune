#include <iostream>
#include <windows.h>
#include <thread>
#include <chrono>
#include "../shared/types.hpp"
#include "sensors.hpp"
#include "actuators.hpp"

using namespace wspa;

void control_loop(TagDatabase& db, ActuatorManager& actuators) {
    while (true) {
        if (db.count("Foreground_App")) {
            auto val = std::get<std::string>(db["Foreground_App"].value);
            std::cout << "[Agent] Current Focus: " << val << std::endl;

            // Simple Logic: If "Notepad" is in focus, try to switch to Power Saver (hypothetical)
            if (val.find("Notepad") != std::string::npos) {
                std::cout << "[Agent] High efficiency app detected. Adjusting power..." << std::endl;
                // GUID for Power Saver: a1841308-3541-4fab-bc81-f71556f20b4a
                // We'll just print for safety in this scaffold.
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

int main() {
    std::cout << "Windows SCADA Power Agent (WSPA) Starting..." << std::endl;

    TagDatabase db;
    SensorManager sensors(db);
    ActuatorManager actuators;

    sensors.start();

    // Start AI control loop in a separate thread
    std::thread ai_thread(control_loop, std::ref(db), std::ref(actuators));
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
