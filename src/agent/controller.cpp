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
        auto db_snapshot = db.get_all();
        
        result.stress_score = calculate_stress_score(db_snapshot);
        
        // Architecture #3: Adaptive Loop Interval
        // Higher stress -> lower interval (faster response)
        double stress_norm = result.stress_score / 100.0;
        result.recommended_interval_ms = (int)(config::MAX_CONTROL_LOOP_INTERVAL_MS - 
            (stress_norm * (config::MAX_CONTROL_LOOP_INTERVAL_MS - config::MIN_CONTROL_LOOP_INTERVAL_MS)));

        if (m_last_state.empty() || is_dirty(db_snapshot)) {
            result.adjustments = compute_adjustments(db_snapshot, result.stress_score);
            
            m_last_state = db_snapshot;
            m_last_stress_score = result.stress_score;
        }

        return result;
    }

    std::map<std::string, double> Controller::compute_adjustments(const std::unordered_map<std::string, Tag>& snapshot, double stress_score) {
        std::map<std::string, double> adjustments;

        float app_hash = 0.0f;
        if (snapshot.count("Foreground_App")) {
            const auto& val = snapshot.at("Foreground_App").value;
            if (std::holds_alternative<std::string>(val)) {
                const std::string& title = std::get<std::string>(val);
                uint32_t hash = 2166136261u;
                for (char c : title) {
                    hash ^= (uint8_t)c;
                    hash *= 16777619u;
                }
                app_hash = (float)(hash % config::APP_HASH_BUCKETS) / (float)config::APP_HASH_BUCKETS;
            }
        }

        std::vector<float> inputs = {
            (float)(snapshot.count("CPU_Utilization") ? 
                (std::holds_alternative<double>(snapshot.at("CPU_Utilization").value) ? std::get<double>(snapshot.at("CPU_Utilization").value) : 0.0) 
                : 0.0),
            (float)(snapshot.count("Thread_Queue_Length") ? 
                (std::holds_alternative<int>(snapshot.at("Thread_Queue_Length").value) ? std::get<int>(snapshot.at("Thread_Queue_Length").value) : 0) 
                : 0),
            (float)stress_score,
            app_hash,
            (float)m_last_stress_score
        };

        bool ai_used = false;

#ifndef WSPA_DISABLE_AI
        if (m_inference) {
            auto outputs = m_inference->run_inference(inputs);
            if (!outputs.empty()) {
                static const std::vector<std::string> OUTPUT_PARAMS = { "PerformanceBoost", "ProcessorFloor" };
                for (size_t i = 0; i < std::min(outputs.size(), OUTPUT_PARAMS.size()); ++i) {
                    adjustments[OUTPUT_PARAMS[i]] = outputs[i];
                }
                ai_used = true;
            }
        }
#endif

        if (!ai_used) {
            // P1 Fix: Improved fallback controller with linear interpolation instead of hardcoded 3-stage thresholds
            if (stress_score >= 70.0) {
                adjustments["PerformanceBoost"] = 100.0;
                adjustments["ProcessorFloor"] = 50.0;
            } else if (stress_score <= 30.0) {
                adjustments["PerformanceBoost"] = 0.0;
                adjustments["ProcessorFloor"] = 0.0;
            } else {
                // Linear scale between 30% and 70% stress
                double ratio = (stress_score - 30.0) / 40.0;
                adjustments["PerformanceBoost"] = ratio * 100.0;
                adjustments["ProcessorFloor"] = ratio * 50.0;
            }
        }

        if (config::DATA_COLLECTION_MODE) {
            log_snapshot(inputs, adjustments["PerformanceBoost"]);
        }

        return adjustments;
    }

    void Controller::log_snapshot(const std::vector<float>& inputs, double label) {
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

    double Controller::calculate_stress_score(const std::unordered_map<std::string, Tag>& db) {
        double cpu = 0.0;
        int queue = 0;
        double thermal = 0.0;
        double gpu = 0.0;
        double disk = 0.0;

        if (db.count("CPU_Utilization")) {
            const auto& val = db.at("CPU_Utilization").value;
            if (std::holds_alternative<double>(val)) cpu = std::get<double>(val);
        }
        if (db.count("Thread_Queue_Length")) {
            const auto& val = db.at("Thread_Queue_Length").value;
            if (std::holds_alternative<int>(val)) queue = std::get<int>(val);
        }
        if (db.count("Thermal_Headroom")) {
            const auto& val = db.at("Thermal_Headroom").value;
            if (std::holds_alternative<double>(val)) thermal = std::get<double>(val);
        }
        if (db.count("GPU_Utilization")) {
            const auto& val = db.at("GPU_Utilization").value;
            if (std::holds_alternative<double>(val)) gpu = std::get<double>(val);
        }
        if (db.count("Disk_Utilization")) {
            const auto& val = db.at("Disk_Utilization").value;
            if (std::holds_alternative<double>(val)) disk = std::get<double>(val);
        }

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

    bool Controller::is_dirty(const std::unordered_map<std::string, Tag>& db) {
        if (m_last_state.size() != db.size()) return true;
        
        for (const auto& [name, tag] : db) {
            if (m_last_state.find(name) == m_last_state.end()) return true;
            
            const auto& last_val = m_last_state.at(name).value;
            const auto& new_val = tag.value;

            if (new_val.index() != last_val.index()) return true;

            if (std::holds_alternative<double>(new_val)) {
                // Implementation #1: Use configured epsilon
                if (std::abs(std::get<double>(new_val) - std::get<double>(last_val)) > config::DIRTY_FLAG_EPSILON) return true;
            } else if (tag.value != last_val) {
                return true;
            }
        }
        return false;
    }
}
