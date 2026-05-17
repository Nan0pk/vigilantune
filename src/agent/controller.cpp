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
        
        // Fix for Significant #9: Ensure evaluation runs on startup
        if (m_last_state.empty() || is_dirty(db_snapshot)) {
            result.stress_score = calculate_stress_score(db_snapshot);
            result.adjustments = compute_adjustments(db_snapshot, result.stress_score);
            
            m_last_state = db_snapshot;
            m_last_stress_score = result.stress_score;
        } else {
            result.stress_score = m_last_stress_score;
        }

        return result;
    }

    // Significant #6: Clean helper instead of goto
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

#ifndef WSPA_DISABLE_AI
        if (m_inference) {
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

            auto outputs = m_inference->run_inference(inputs);
            if (!outputs.empty()) {
                // Fix for Significant #12: Multi-parameter mapping
                static const std::vector<std::string> OUTPUT_PARAMS = { "PerformanceBoost", "ProcessorFloor" };
                for (size_t i = 0; i < std::min(outputs.size(), OUTPUT_PARAMS.size()); ++i) {
                    adjustments[OUTPUT_PARAMS[i]] = outputs[i];
                }
                return adjustments;
            }
        }
#endif

        // Fallback to mock logic
        if (stress_score > 70.0) adjustments["PerformanceBoost"] = 100.0;
        else if (stress_score < 30.0) adjustments["PerformanceBoost"] = 0.0;
        else adjustments["PerformanceBoost"] = 50.0;

        return adjustments;
    }

    double Controller::calculate_stress_score(const std::unordered_map<std::string, Tag>& db) {
        double cpu = 0.0;
        int queue = 0;
        double thermal = 0.0;

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

        // Fix for Significant #7: Logarithmic scaling for the Thread Queue
        // queue 1->6->50 maps to ~17->50->100
        double queue_norm = std::clamp(std::log1p(queue) / std::log1p(50.0) * 100.0, 0.0, 100.0);
        
        // System Stress Score (SSS) calculation
        double thermal_pressure = std::clamp((thermal - 60.0) / 40.0, 0.0, 1.0) * 100.0;
        double sss = (cpu * 0.35) + (queue_norm * 0.50) + (thermal_pressure * 0.15);
        
        return std::clamp(sss, 0.0, 100.0);
    }

    bool Controller::is_dirty(const std::unordered_map<std::string, Tag>& db) {
        if (m_last_state.size() != db.size()) return true;
        
        const double EPSILON = 1.0; // 1.0% change threshold

        for (const auto& [name, tag] : db) {
            if (m_last_state.find(name) == m_last_state.end()) return true;
            
            const auto& last_val = m_last_state.at(name).value;
            const auto& new_val = tag.value;

            if (new_val.index() != last_val.index()) return true;

            if (std::holds_alternative<double>(new_val)) {
                if (std::abs(std::get<double>(new_val) - std::get<double>(last_val)) > EPSILON) return true;
            } else if (std::holds_alternative<int>(new_val)) {
                if (std::get<int>(new_val) != std::get<int>(last_val)) return true;
            } else if (std::holds_alternative<std::string>(new_val)) {
                if (std::get<std::string>(new_val) != std::get<std::string>(last_val)) return true;
            } else if (std::holds_alternative<bool>(new_val)) {
                if (std::get<bool>(new_val) != std::get<bool>(last_val)) return true;
            }
        }
        return false;
    }
}
