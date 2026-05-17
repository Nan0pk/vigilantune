#include "controller.hpp"
#include <iostream>
#include <variant>

namespace wspa {
    Controller::Controller() : m_last_stress_score(0.0) {}
    Controller::~Controller() {}

    ControlResult Controller::evaluate(const TagDatabase& db) {
        ControlResult result;
        result.stress_score = calculate_stress_score(db);

        if (!is_dirty(db)) {
            return result;
        }

        // Mock adjustments based on stress
        if (result.stress_score > 70.0) {
            result.adjustments["PerformanceBoost"] = 100.0; 
        } else if (result.stress_score < 30.0) {
            result.adjustments["PerformanceBoost"] = 0.0;
        } else {
            result.adjustments["PerformanceBoost"] = 50.0;
        }

        m_last_state = db;
        m_last_stress_score = result.stress_score;

        return result;
    }

    double Controller::calculate_stress_score(const TagDatabase& db) {
        double cpu = 0.0;
        int queue = 0;

        if (db.count("CPU_Utilization")) {
            cpu = std::get<double>(db.at("CPU_Utilization").value);
        }
        if (db.count("Thread_Queue_Length")) {
            queue = std::get<int>(db.at("Thread_Queue_Length").value);
        }

        // System Stress Score (SSS) calculation
        // Exponential weight on Queue depth as a leading indicator
        double sss = (cpu * 0.4) + (std::min(queue * 10, 60) * 0.6);
        
        return std::clamp(sss, 0.0, 100.0);
    }

    bool Controller::is_dirty(const TagDatabase& db) {
        if (m_last_state.size() != db.size()) return true;
        
        // Check for significant changes in metrics
        for (const auto& [name, tag] : db) {
            if (m_last_state.find(name) == m_last_state.end()) return true;
            
            // For now, if value is different, it's dirty
            if (tag.value != m_last_state.at(name).value) return true;
        }
        
        return false;
    }
}
