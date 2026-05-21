#include "controller.hpp"
#include <variant>
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include "../shared/config.hpp"
#include "../shared/logger.hpp"

namespace nanoloop {
    Controller::Controller() : m_last_stress_score(0.0) {
#ifndef NANOLOOP_DISABLE_AI
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

#ifndef NANOLOOP_DISABLE_AI
        if (m_inference) {
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
            if (stress_score >= 70.0) {
                adjustments.set(ActuatorID::PerformanceBoost, 100.0);
                adjustments.set(ActuatorID::ProcessorFloor, 50.0);
            } else if (stress_score <= 30.0) {
                adjustments.set(ActuatorID::PerformanceBoost, 0.0);
                adjustments.set(ActuatorID::ProcessorFloor, 0.0);
            } else {
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

    static uint64_t get_file_size(const std::string& path) {
        std::ifstream in(path, std::ifstream::ate | std::ifstream::binary);
        if (!in.is_open()) return 0;
        return in.tellg();
    }

    void Controller::log_snapshot(const std::array<float, 5>& inputs, double label) {
        std::string filepath = config::TELEMETRY_LOG_PATH;
        uint64_t max_bytes = static_cast<uint64_t>(config::MAX_FILE_SIZE_MB) * 1024 * 1024;

        // Perform log rotation if file size limit exceeded
        if (get_file_size(filepath) >= max_bytes) {
            int max_idx = config::MAX_ROTATED_FILES;
            auto dot = filepath.rfind('.');
            if (dot != std::string::npos) {
                std::string oldest = filepath.substr(0, dot) + "_" + std::to_string(max_idx) + filepath.substr(dot);
                std::remove(oldest.c_str());

                for (int i = max_idx; i >= 1; --i) {
                    std::string src;
                    if (i == 1) {
                        src = filepath;
                    } else {
                        src = filepath.substr(0, dot) + "_" + std::to_string(i - 1) + filepath.substr(dot);
                    }
                    
                    std::string dst = filepath.substr(0, dot) + "_" + std::to_string(i) + filepath.substr(dot);
                    std::rename(src.c_str(), dst.c_str());
                }
            }
            LOG_INFO("Controller", "Telemetry log rotated successfully.");
        }

        static bool header_checked = false;
        static bool header_needed = false;
        if (!header_checked) {
            uint64_t size = get_file_size(filepath);
            header_needed = (size == 0);
            header_checked = true;
        }

        std::ofstream file(filepath, std::ios::app);
        if (!file.is_open()) return;

        if (header_needed) {
            file << "CPU_Utilization,Thread_Queue_Length,Stress_Score,App_Hash,Last_Adjustment,PerformanceBoost_Label\n";
            header_needed = false;
        }

        for (size_t i = 0; i < inputs.size(); ++i) {
            file << inputs[i] << ",";
        }
        file << label << "\n";
    }

    double Controller::calculate_stress_score(const TagSnapshot& db) {
        double cpu = db.values[static_cast<size_t>(TagID::CPU_Utilization)];
        int queue = (int)db.values[static_cast<size_t>(TagID::Thread_Queue_Length)];
        double thermal = db.values[static_cast<size_t>(TagID::Thermal_Headroom)];
        double gpu = db.values[static_cast<size_t>(TagID::GPU_Utilization)];
        double disk = db.values[static_cast<size_t>(TagID::Disk_Utilization)];

        double queue_norm = std::clamp(std::log1p(queue) / std::log1p(50.0) * 100.0, 0.0, 100.0);
        double thermal_pressure = std::clamp((thermal - 60.0) / 40.0, 0.0, 1.0) * 100.0;
        
        double sss = (cpu * config::SSS_CPU_WEIGHT) + 
                     (queue_norm * config::SSS_QUEUE_WEIGHT) + 
                     (thermal_pressure * config::SSS_THERMAL_WEIGHT) +
                     (gpu * config::SSS_GPU_WEIGHT) +
                     (disk * config::SSS_DISK_WEIGHT);
        
        return std::clamp(sss, 0.0, 100.0);
    }
} // namespace nanoloop
