#pragma once
#include <map>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <fstream>
#include "../shared/types.hpp"
#include "ecu_engine.hpp"

#ifndef NANOLOOP_DISABLE_AI
#include "inference.hpp"
#endif

namespace vigilantune {
    struct ControlResult {
        ActuationSet adjustments;
        double stress_score;
        int recommended_interval_ms;
    };

    class Controller {
    public:
        Controller();
        ~Controller();

        ControlResult evaluate(const TagDatabase& db);

    protected:
        double calculate_stress_score(const TagSnapshot& db);
        
        void log_snapshot(const std::array<float, 5>& inputs, double label);
        ActuationSet compute_adjustments(const TagSnapshot& snapshot, double stress_score);

        TagSnapshot m_last_state;
        bool m_first_run{true};
        double m_last_stress_score;
        std::array<float, 5> m_inference_inputs;

        std::vector<ECUTable2D> m_ecu_tables;

#ifndef NANOLOOP_DISABLE_AI
        std::unique_ptr<nanoloop::InferenceManager> m_inference;
#endif
    };
} // namespace vigilantune
