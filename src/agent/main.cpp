#include <iostream>
#include <windows.h>
#include <thread>
#include <chrono>
#include "../shared/types.hpp"
#include "sensors.hpp"

using namespace wspa;

void control_loop(TagDatabase& db) {
    while (true) {
        // AI Control Loop Logic
        // For now, just print the current foreground app from the Tag DB
        if (db.count("Foreground_App")) {
            auto val = std::get<std::string>(db["Foreground_App"].value);
            std::cout << "[Agent] Current Focus: " << val << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

int main() {
    std::cout << "Windows SCADA Power Agent (WSPA) Starting..." << std::endl;

    TagDatabase db;
    SensorManager sensors(db);

    sensors.start();

    // Start AI control loop in a separate thread
    std::thread ai_thread(control_loop, std::ref(db));
    ai_thread.detach();

    // Standard Win32 Message Loop (Required for WinEventHooks)
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    sensors.stop();
    return 0;
}
