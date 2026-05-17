#include <gtest/gtest.h>
#include "../src/agent/actuators.hpp"
#include <iostream>

using namespace wspa;

class TestActuator : public ActuatorManager {
public:
    using ActuatorManager::calculate_deadband;
    using ActuatorManager::should_apply;
    using ActuatorManager::m_last_applied_values;
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
    
    // Initial state (manual injection to bypass Win32 dependency)
    actuator.m_last_applied_values["PerformanceBoost"] = 50.0;
    
    // Small change (3%) - should NOT apply with 5% deadband
    EXPECT_FALSE(actuator.should_apply("PerformanceBoost", 53.0, 5.0));
    
    // Large change (6%) - should apply with 5% deadband
    EXPECT_TRUE(actuator.should_apply("PerformanceBoost", 56.0, 5.0));
    
    // Switch to High Stress (1% deadband)
    // Small change (1.5%) - should apply now
    EXPECT_TRUE(actuator.should_apply("PerformanceBoost", 51.5, 1.0));
}
