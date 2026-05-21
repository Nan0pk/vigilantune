#include <gtest/gtest.h>
#include "../src/agent/sensors.hpp"
#include "../src/agent/actuators.hpp"
#include "../src/agent/controller.hpp"
#include "../src/shared/types.hpp"
#include <thread>
#include <chrono>

using namespace nanoloop;

TEST(PipelineTest, FullFlow) {
    TagDatabase db;
    SensorManager sensors(db);
    ActuatorManager actuators;
    Controller controller;

    // 1. Simulate Sensor Data
    db.set(TagID::CPU_Utilization, 85.0);
    db.set(TagID::Thread_Queue_Length, 10.0);
    db.set(TagID::Thermal_Headroom, 75.0);
    db.set(TagID::GPU_Utilization, 20.0);
    db.set(TagID::Disk_Utilization, 5.0);

    // 2. Run Controller
    auto result = controller.evaluate(db);
    
    // High stress (85% CPU + Queue + Thermal) should trigger adjustments
    EXPECT_GT(result.stress_score, 50.0);
    EXPECT_FALSE(result.adjustments.empty());

    // 3. Apply Actuations
    actuators.queue_adjustments(result.adjustments);
    
    // This calls Win32 Power APIs, so we just verify it doesn't crash
    // and returns true (success) if possible.
    bool committed = actuators.commit_changes(result.stress_score);
    EXPECT_TRUE(committed);
}
