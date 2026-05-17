#include "controller.hpp"
#include <iostream>

namespace wspa {
    Controller::Controller() : m_last_stress_score(0.0) {}
    Controller::~Controller() {}

    std::map<std::string, double> Controller::evaluate(const TagDatabase& db) {
        std::map<std::string, double> adjustments;

        if (!is_dirty(db)) {
            // Optimization: Skip inference if state hasn't changed
            return adjustments;
        }

        double stress = calculate_stress_score(db);
        std::cout << "[Controller] Evaluation triggered. Stress Score: " << stress << std::endl;

        // Placeholder for ONNX Inference
        // inputs = prepare_inputs(db);
        // outputs = onnx_session.run(inputs);
        
        // Mock output: decrease power if stress is low, increase if high
        if (stress > 70.0) {
            adjustments["PerformanceBoost"] = 1.0; 
        } else if (stress < 30.0) {
            adjustments["PerformanceBoost"] = 0.0;
        }

        m_last_state = db;
        m_last_stress_score = stress;

        return adjustments;
    }

    double Controller::calculate_stress_score(const TagDatabase& db) {
        // Mock stress score calculation
        // Real version would weight CPU queue, thermal headroom, etc.
        return 50.0; 
    }

    bool Controller::is_dirty(const TagDatabase& db) {
        if (m_last_state.size() != db.size()) return true;
        
        // Simple comparison for demo
        for (const auto& [name, tag] : db) {
            if (m_last_state.find(name) == m_last_state.end()) return true;
            // In a real version, we'd hash the values
        }
        
        return false;
    }
}
