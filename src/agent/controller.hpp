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
        ActuationSet adjustments;
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
        double calculate_stress_score(const TagSnapshot& db);
        
        // Phase 1: Data Collection Helper
        void log_snapshot(const std::array<float, 5>& inputs, double label);

        // Logic helper for adjustments (Significant #6)
        ActuationSet compute_adjustments(const TagSnapshot& snapshot, double stress_score);

        TagSnapshot m_last_state;
        bool m_first_run{true};
        double m_last_stress_score;
        std::array<float, 5> m_inference_inputs; // Pre-allocated vector for zero-allocation AI inputs

#ifndef WSPA_DISABLE_AI
        std::unique_ptr<InferenceManager> m_inference;
#endif
    };
}
