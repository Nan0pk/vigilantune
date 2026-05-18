#include <gtest/gtest.h>
#include "../src/agent/sensors.hpp"
#include "../shared/types.hpp"

using namespace wspa;

TEST(SensorTest, Lifecycle) {
    TagDatabase db;
    SensorManager sensors(db);
    
    // Test start/stop doesn't crash
    sensors.start();
    sensors.stop();
}

TEST(SensorTest, MetricsCollectionGracefulFailure) {
    TagDatabase db;
    SensorManager sensors(db);
    
    // Even if not started, shouldn't crash
    sensors.collect_performance_metrics();
    sensors.collect_high_fidelity_metrics();
}
