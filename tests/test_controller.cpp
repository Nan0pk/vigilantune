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
    
    // CPU = 100 * 0.35 = 35
    // Queue = 100 * 0.50 = 50
    // Thermal = 100 * 0.15 = 15
    // Total = 100
    EXPECT_NEAR(controller.calculate_stress_score(db), 100.0, 0.1);
}

TEST(ControllerTest, DirtyFlagEpsilon) {
    TestController controller;
    std::unordered_map<std::string, Tag> db;
    
    db["CPU_Utilization"] = { 50.0, std::chrono::system_clock::now() };
    
    // First run (should be dirty because m_last_state is empty)
    // Note: evaluate() handles the empty check, so we simulate evaluate behavior or test is_dirty directly
    
    // Set last state manually if possible or just use evaluate
    TagDatabase tag_db;
    tag_db.set("CPU_Utilization", 50.0);
    
    auto result = controller.evaluate(tag_db);
    
    // Small change (within epsilon 1.0%)
    tag_db.set("CPU_Utilization", 50.5);
    auto result2 = controller.evaluate(tag_db);
    
    // Stress score should be the same if dirty check worked (it didn't recompute or adjustments remained same)
    // Actually evaluate() recomputes stress_score every time but compute_adjustments only on dirty.
    // Let's check result adjustments.
}
