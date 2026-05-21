#include <gtest/gtest.h>
#include "../src/agent/controller.hpp"
#include "../src/shared/config.hpp"
#include "../src/shared/types.hpp"

using namespace nanoloop;

class TestController : public Controller {
public:
    using Controller::calculate_stress_score;
};

TEST(ControllerTest, SSSCalculation) {
    TestController controller;
    TagSnapshot db = {};
    
    // Case 1: Minimal load
    db.values[static_cast<size_t>(TagID::CPU_Utilization)] = 0.0;
    db.values[static_cast<size_t>(TagID::Thread_Queue_Length)] = 0.0;
    db.values[static_cast<size_t>(TagID::Thermal_Headroom)] = 40.0;
    
    // SSS = (0 * 0.35) + (log1p(0)/log1p(50)*100 * 0.5) + (max(0, (40-60)/40)*100 * 0.15) = 0
    EXPECT_NEAR(controller.calculate_stress_score(db), 0.0, 0.1);
    
    // Case 2: High load
    db.values[static_cast<size_t>(TagID::CPU_Utilization)] = 100.0;
    db.values[static_cast<size_t>(TagID::Thread_Queue_Length)] = 50.0;
    db.values[static_cast<size_t>(TagID::Thermal_Headroom)] = 100.0;
    db.values[static_cast<size_t>(TagID::GPU_Utilization)] = 100.0;
    db.values[static_cast<size_t>(TagID::Disk_Utilization)] = 100.0;
    
    // Total = 100
    EXPECT_NEAR(controller.calculate_stress_score(db), 100.0, 0.1);
}

TEST(ControllerTest, DirtyFlagEpsilon) {
    TestController controller;
    TagDatabase tag_db;
    
    tag_db.set(TagID::CPU_Utilization, 50.0);
    
    auto result = controller.evaluate(tag_db);
    
    // Small change (within epsilon 1.0%)
    tag_db.set(TagID::CPU_Utilization, 50.5);
    auto result2 = controller.evaluate(tag_db);
    
    // Adjustments should NOT have been recomputed, so they should be empty
    EXPECT_TRUE(result2.adjustments.empty());
    
    // Large change (outside epsilon 1.0%)
    tag_db.set(TagID::CPU_Utilization, 55.0);
    auto result3 = controller.evaluate(tag_db);
    
    // Adjustments SHOULD be present
    EXPECT_FALSE(result3.adjustments.empty());
}
