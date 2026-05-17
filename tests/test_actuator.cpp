#include <gtest/gtest.h>
#include "../src/agent/actuators.hpp"
#include <iostream>

using namespace wspa;

class TestActuator : public ActuatorManager {
public:
    using ActuatorManager::calculate_deadband;
    using ActuatorManager::should_apply;
};

TEST(ActuatorTest, DeadbandCalculation) {
    TestActuator actuator;
    
    // High Stress
    EXPECT_DOUBLE_EQ(actuator.calculate_deadband(80.0), 1.0);
    // Medium Stress
    EXPECT_DOUBLE_EQ(actuator.calculate_deadband(50.0), 5.0);
    // Low Stress
    EXPECT_DOUBLE_EQ(actuator.calculate_deadband(10.0), 10.0);
}

TEST(ActuatorTest, AdaptiveFiltering) {
    TestActuator actuator;
    
    // Initial state
    actuator.queue_adjustment("PerformanceBoost", 50.0);
    actuator.commit_changes(50.0); // Medium stress, 5% deadband
    
    // Small change (3%) - should NOT apply
    EXPECT_FALSE(actuator.should_apply("PerformanceBoost", 53.0, 5.0));
    
    // Large change (6%) - should apply
    EXPECT_TRUE(actuator.should_apply("PerformanceBoost", 56.0, 5.0));
    
    // Switch to High Stress (1% deadband)
    // Small change (1.5%) - should apply now
    EXPECT_TRUE(actuator.should_apply("PerformanceBoost", 51.5, 1.0));
}
