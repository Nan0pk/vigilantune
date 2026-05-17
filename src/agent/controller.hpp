#pragma once
#include <map>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <fstream>
#include "../shared/types.hpp"

// Fix for Critical #2: Guard ONNX header include
#ifndef WSPA_DISABLE_AI
#include "inference.hpp"
#endif

namespace wspa {
    struct ControlResult {
        std::map<std::string, double> adjustments;
        double stress_score;
        int recommended_interval_ms; // Added for Adaptive Loop (Architecture #3)
    };

    class Controller {
    public:
        Controller();
        ~Controller();

        // Evaluates the current state and returns relative changes + stress score
        ControlResult evaluate(const TagDatabase& db);

    protected:
        double calculate_stress_score(const std::unordered_map<std::string, Tag>& db);
        bool is_dirty(const std::unordered_map<std::string, Tag>& db);
        
        // Phase 1: Data Collection Helper
        void log_snapshot(const std::vector<float>& inputs, double label);

        // Logic helper for adjustments (Significant #6)
        std::map<std::string, double> compute_adjustments(const std::unordered_map<std::string, Tag>& snapshot, double stress_score);

        std::unordered_map<std::string, Tag> m_last_state;
        double m_last_stress_score;

#ifndef WSPA_DISABLE_AI
        std::unique_ptr<InferenceManager> m_inference;
#endif
    };
}
