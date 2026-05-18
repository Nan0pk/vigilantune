#include "controller.hpp"
#include <iostream>
#include <variant>
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include "../shared/config.hpp"

namespace wspa {
    Controller::Controller() : m_last_stress_score(0.0) {
#ifndef WSPA_DISABLE_AI
        m_inference = std::make_unique<InferenceManager>(config::MODEL_PATH);
#endif
    }

    Controller::~Controller() {}

    ControlResult Controller::evaluate(const TagDatabase& db) {
        ControlResult result;
        auto db_snapshot = db.get_snapshot();
        
        result.stress_score = calculate_stress_score(db_snapshot);
        
        // Architecture #3: Adaptive Loop Interval
        // Higher stress -> lower interval (faster response)
        double stress_norm = result.stress_score / 100.0;
        result.recommended_interval_ms = (int)(config::MAX_CONTROL_LOOP_INTERVAL_MS - 
            (stress_norm * (config::MAX_CONTROL_LOOP_INTERVAL_MS - config::MIN_CONTROL_LOOP_INTERVAL_MS)));

        if (m_first_run || db_snapshot.is_dirty(m_last_state, config::DIRTY_FLAG_EPSILON)) {
            result.adjustments = compute_adjustments(db_snapshot, result.stress_score);
            
            m_last_state = db_snapshot;
            m_last_stress_score = result.stress_score;
            m_first_run = false;
        }

        return result;
    }

    ActuationSet Controller::compute_adjustments(const TagSnapshot& snapshot, double stress_score) {
        ActuationSet adjustments;

        float app_hash = (float)snapshot.values[static_cast<size_t>(TagID::Foreground_App_Hash)];

        m_inference_inputs[0] = (float)snapshot.values[static_cast<size_t>(TagID::CPU_Utilization)];
        m_inference_inputs[1] = (float)snapshot.values[static_cast<size_t>(TagID::Thread_Queue_Length)];
        m_inference_inputs[2] = (float)stress_score;
        m_inference_inputs[3] = app_hash;
        m_inference_inputs[4] = (float)m_last_stress_score;

        bool ai_used = false;

#ifndef WSPA_DISABLE_AI
        if (m_inference) {
            // InferenceManager expects vector for now. We will optimize InferenceManager later.
            std::vector<float> in_vec(m_inference_inputs.begin(), m_inference_inputs.end());
            auto outputs = m_inference->run_inference(in_vec);
            if (!outputs.empty()) {
                if (outputs.size() > 0) adjustments.set(ActuatorID::PerformanceBoost, outputs[0]);
                if (outputs.size() > 1) adjustments.set(ActuatorID::ProcessorFloor, outputs[1]);
                ai_used = true;
            }
        }
#endif

        if (!ai_used) {
            // P1 Fix: Improved fallback controller with linear interpolation instead of hardcoded 3-stage thresholds
            if (stress_score >= 70.0) {
                adjustments.set(ActuatorID::PerformanceBoost, 100.0);
                adjustments.set(ActuatorID::ProcessorFloor, 50.0);
            } else if (stress_score <= 30.0) {
                adjustments.set(ActuatorID::PerformanceBoost, 0.0);
                adjustments.set(ActuatorID::ProcessorFloor, 0.0);
            } else {
                // Linear scale between 30% and 70% stress
                double ratio = (stress_score - 30.0) / 40.0;
                adjustments.set(ActuatorID::PerformanceBoost, ratio * 100.0);
                adjustments.set(ActuatorID::ProcessorFloor, ratio * 50.0);
            }
        }

        if (config::DATA_COLLECTION_MODE) {
            log_snapshot(m_inference_inputs, adjustments.values[static_cast<size_t>(ActuatorID::PerformanceBoost)]);
        }

        return adjustments;
    }

    void Controller::log_snapshot(const std::array<float, 5>& inputs, double label) {
        static bool header_written = false;
        std::ofstream file(config::TELEMETRY_LOG_PATH, std::ios::app);
        
        if (!file.is_open()) return;

        if (!header_written) {
            file.seekp(0, std::ios::end);
            if (file.tellp() == 0) {
                file << "CPU_Utilization,Thread_Queue_Length,Stress_Score,App_Hash,Last_Adjustment,PerformanceBoost_Label\n";
            }
            header_written = true;
        }

        for (size_t i = 0; i < inputs.size(); ++i) {
            file << inputs[i] << (i == inputs.size() - 1 ? "" : ",");
        }
        file << "," << label << "\n";
    }

    double Controller::calculate_stress_score(const TagSnapshot& db) {
        double cpu = db.values[static_cast<size_t>(TagID::CPU_Utilization)];
        int queue = (int)db.values[static_cast<size_t>(TagID::Thread_Queue_Length)];
        double thermal = db.values[static_cast<size_t>(TagID::Thermal_Headroom)];
        double gpu = db.values[static_cast<size_t>(TagID::GPU_Utilization)];
        double disk = db.values[static_cast<size_t>(TagID::Disk_Utilization)];

        double queue_norm = std::clamp(std::log1p(queue) / std::log1p(50.0) * 100.0, 0.0, 100.0);
        double thermal_pressure = std::clamp((thermal - 60.0) / 40.0, 0.0, 1.0) * 100.0;
        
        // Implementation #1: Use configured weights
        double sss = (cpu * config::SSS_CPU_WEIGHT) + 
                     (queue_norm * config::SSS_QUEUE_WEIGHT) + 
                     (thermal_pressure * config::SSS_THERMAL_WEIGHT) +
                     (gpu * config::SSS_GPU_WEIGHT) +
                     (disk * config::SSS_DISK_WEIGHT);
        
        return std::clamp(sss, 0.0, 100.0);
    }
}
