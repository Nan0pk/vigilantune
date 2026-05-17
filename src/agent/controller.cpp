#include "controller.hpp"
#include <iostream>
#include <variant>
#include <algorithm>
#include <cmath>
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

        if (!is_dirty(db_snapshot)) {
            return result;
        }

        float app_hash = 0.0f;
        if (db_snapshot.count("Foreground_App")) {
            const auto& val = db_snapshot.at("Foreground_App").value;
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
        std::vector<float> inputs = {
            (float)(db_snapshot.count("CPU_Utilization") ? 
                (std::holds_alternative<double>(db_snapshot.at("CPU_Utilization").value) ? std::get<double>(db_snapshot.at("CPU_Utilization").value) : 0.0) 
                : 0.0),
            (float)(db_snapshot.count("Thread_Queue_Length") ? 
                (std::holds_alternative<int>(db_snapshot.at("Thread_Queue_Length").value) ? std::get<int>(db_snapshot.at("Thread_Queue_Length").value) : 0) 
                : 0),
            (float)result.stress_score,
            app_hash,
            (float)m_last_stress_score
        };

        if (m_inference) {
            auto outputs = m_inference->run_inference(inputs);
            if (!outputs.empty()) {
                result.adjustments["PerformanceBoost"] = outputs[0];
                goto cleanup;
            }
        }
#endif

        if (result.stress_score > 70.0) result.adjustments["PerformanceBoost"] = 100.0;
        else if (result.stress_score < 30.0) result.adjustments["PerformanceBoost"] = 0.0;
        else result.adjustments["PerformanceBoost"] = 50.0;

#ifndef WSPA_DISABLE_AI
cleanup:
#endif
        m_last_state = db_snapshot;
        m_last_stress_score = result.stress_score;
        return result;
    }

    double Controller::calculate_stress_score(const std::map<std::string, Tag>& db) {
        double cpu = 0.0;
        int queue = 0;

        if (db.count("CPU_Utilization")) {
            const auto& val = db.at("CPU_Utilization").value;
            if (std::holds_alternative<double>(val)) cpu = std::get<double>(val);
        }
        if (db.count("Thread_Queue_Length")) {
            const auto& val = db.at("Thread_Queue_Length").value;
            if (std::holds_alternative<int>(val)) queue = std::get<int>(val);
        }

        double sss = (cpu * 0.4) + (std::min(queue * 10, 60) * 0.6);
        return std::clamp(sss, 0.0, 100.0);
    }

    bool Controller::is_dirty(const std::map<std::string, Tag>& db) {
        if (m_last_state.size() != db.size()) return true;
        
        const double EPSILON = 0.1;

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
