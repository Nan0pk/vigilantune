#include <gtest/gtest.h>
#include "../src/agent/controller.hpp"
#include "../src/shared/config.hpp"
#include <unordered_map>

using namespace wspa;

class TestController : public Controller {
public:
    using Controller::calculate_stress_score;
    using Controller::is_dirty;
};

TEST(ControllerTest, SSSCalculation) {
    TestController controller;
    std::unordered_map<std::string, Tag> db;
    
    // Case 1: Minimal load
    db["CPU_Utilization"] = { 0.0, std::chrono::system_clock::now() };
    db["Thread_Queue_Length"] = { 0, std::chrono::system_clock::now() };
    db["Thermal_Headroom"] = { 40.0, std::chrono::system_clock::now() };
    
    // SSS = (0 * 0.35) + (log1p(0)/log1p(50)*100 * 0.5) + (max(0, (40-60)/40)*100 * 0.15) = 0
    EXPECT_NEAR(controller.calculate_stress_score(db), 0.0, 0.1);
    
    // Case 2: High load
    db["CPU_Utilization"] = { 100.0, std::chrono::system_clock::now() };
    db["Thread_Queue_Length"] = { 50, std::chrono::system_clock::now() };
    db["Thermal_Headroom"] = { 100.0, std::chrono::system_clock::now() };
    db["GPU_Utilization"] = { 100.0, std::chrono::system_clock::now() };
    db["Disk_Utilization"] = { 100.0, std::chrono::system_clock::now() };
    
    // CPU = 100 * 0.30 = 30
    // Queue = 100 * 0.40 = 40
    // Thermal = 100 * 0.10 = 10
    // GPU = 100 * 0.10 = 10
    // Disk = 100 * 0.10 = 10
    // Total = 100
    EXPECT_NEAR(controller.calculate_stress_score(db), 100.0, 0.1);
}

TEST(ControllerTest, DirtyFlagEpsilon) {
    TestController controller;
    TagDatabase tag_db;
    
    tag_db.set("CPU_Utilization", 50.0);
    
    auto result = controller.evaluate(tag_db);
    double initial_sss = result.stress_score;
    
    // Small change (within epsilon 1.0%)
    tag_db.set("CPU_Utilization", 50.5);
    auto result2 = controller.evaluate(tag_db);
    
    // Adjustments should NOT have been recomputed, so they should be empty (since m_last_state was updated but is_dirty was false)
    // Actually evaluate() only returns result.adjustments if is_dirty is true.
    EXPECT_TRUE(result2.adjustments.empty());
    
    // Large change (outside epsilon 1.0%)
    tag_db.set("CPU_Utilization", 55.0);
    auto result3 = controller.evaluate(tag_db);
    
    // Adjustments SHOULD be present
    EXPECT_FALSE(result3.adjustments.empty());
}
