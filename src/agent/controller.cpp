#include "controller.hpp"
#include <iostream>
#include <variant>

namespace wspa {
    Controller::Controller() : m_last_stress_score(0.0) {
        // In production, this path would be absolute or relative to the executable
        m_inference = std::make_unique<InferenceManager>(L"models/power_model.onnx");
    }

    Controller::~Controller() {}

    ControlResult Controller::evaluate(const TagDatabase& db) {
        ControlResult result;
        result.stress_score = calculate_stress_score(db);

        if (!is_dirty(db)) {
            return result;
        }

        // 3. Foreground App Hashing (FNV-1a)
        float app_hash = 0.0f;
        if (db.count("Foreground_App")) {
            const std::string& title = std::get<std::string>(db.at("Foreground_App").value);
            uint32_t hash = 2166136261u;
            for (char c : title) {
                hash ^= (uint8_t)c;
                hash *= 16777619u;
            }
            // Normalize hash for model input (0.0 to 1.0 range)
            app_hash = (float)(hash % 1000) / 1000.0f;
        }

        // Prepare input tensor for ONNX: [CPU, Queue, Stress, Foreground_Hash, Last_Adjustment]
        std::vector<float> inputs = {
            (float)(db.count("CPU_Utilization") ? std::get<double>(db.at("CPU_Utilization").value) : 0.0),
            (float)(db.count("Thread_Queue_Length") ? std::get<int>(db.at("Thread_Queue_Length").value) : 0),
            (float)result.stress_score,
            app_hash,
            (float)m_last_stress_score
        };

        auto outputs = m_inference->run_inference(inputs);

        if (!outputs.empty()) {
            // Map model output (0.0 to 100.0) to PerformanceBoost
            result.adjustments["PerformanceBoost"] = outputs[0];
        } else {
            // Fallback to mock logic if inference fails
            if (result.stress_score > 70.0) result.adjustments["PerformanceBoost"] = 100.0;
            else if (result.stress_score < 30.0) result.adjustments["PerformanceBoost"] = 0.0;
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
