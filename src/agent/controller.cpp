#include "controller.hpp"
#include <variant>
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include "../shared/config.hpp"
#include "../shared/logger.hpp"

namespace vigilantune {
    Controller::Controller() : m_last_stress_score(0.0) {
#ifndef NANOLOOP_DISABLE_AI
        m_inference = std::make_unique<nanoloop::InferenceManager>(nanoloop::config::MODEL_PATH);
#endif
        m_ecu_tables.push_back(ECUMapRegistry::get_default_epp_map());
        m_ecu_tables.push_back(ECUMapRegistry::get_default_timer_map());
        m_ecu_tables.push_back(ECUMapRegistry::get_default_cooling_map());
    }

    Controller::~Controller() {}

    ControlResult Controller::evaluate(const TagDatabase& db) {
        ControlResult result;
        auto db_snapshot = db.get_snapshot();
        
        result.stress_score = calculate_stress_score(db_snapshot);
        
        double stress_norm = result.stress_score / 100.0;
        result.recommended_interval_ms = (int)(nanoloop::config::MAX_CONTROL_LOOP_INTERVAL_MS - 
            (stress_norm * (nanoloop::config::MAX_CONTROL_LOOP_INTERVAL_MS - nanoloop::config::MIN_CONTROL_LOOP_INTERVAL_MS)));

        if (m_first_run || db_snapshot.is_dirty(m_last_state, nanoloop::config::DIRTY_FLAG_EPSILON)) {
            result.adjustments = compute_adjustments(db_snapshot, result.stress_score);
            
            m_last_state = db_snapshot;
            m_last_stress_score = result.stress_score;
            m_first_run = false;
        }

        return result;
    }

    ActuationSet Controller::compute_adjustments(const TagSnapshot& snapshot, double stress_score) {
        ActuationSet adjustments;

        // 1. Bilinear ECU Mapping Engine Calculations
        for (const auto& table : m_ecu_tables) {
            double x_val = snapshot.values[static_cast<size_t>(table.x_axis_tag)];
            double y_val = snapshot.values[static_cast<size_t>(table.y_axis_tag)];
            
            double target_out = table.lookup(x_val, y_val);
            adjustments.set(table.target_actuator, target_out);
        }

        // 2. Backward Compatibility / Inference Fallback Block
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

        if (nanoloop::config::DATA_COLLECTION_MODE) {
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
        std::string filepath = nanoloop::config::TELEMETRY_LOG_PATH;
        uint64_t max_bytes = static_cast<uint64_t>(nanoloop::config::MAX_FILE_SIZE_MB) * 1024 * 1024;

        if (get_file_size(filepath) >= max_bytes) {
            int max_idx = nanoloop::config::MAX_ROTATED_FILES;
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
        
        double ram = db.values[static_cast<size_t>(TagID::Memory_Utilization)];
        double battery_rate = db.values[static_cast<size_t>(TagID::Battery_Power_Rate)];

        double queue_norm = std::clamp(std::log1p(queue) / std::log1p(50.0) * 100.0, 0.0, 100.0);
        double thermal_pressure = std::clamp((thermal - 60.0) / 40.0, 0.0, 1.0) * 100.0;
        
        double battery_drain_factor = battery_rate < 0.0 ? std::clamp(std::abs(battery_rate) / 25000.0 * 100.0, 0.0, 100.0) : 0.0;

        double sss = (cpu * nanoloop::config::SSS_CPU_WEIGHT) + 
                     (queue_norm * nanoloop::config::SSS_QUEUE_WEIGHT) + 
                     (thermal_pressure * nanoloop::config::SSS_THERMAL_WEIGHT) +
                     (gpu * nanoloop::config::SSS_GPU_WEIGHT) +
                     (disk * nanoloop::config::SSS_DISK_WEIGHT) +
                     (ram * 0.05) + 
                     (battery_drain_factor * 0.05);
        
        return std::clamp(sss, 0.0, 100.0);
    }
} // namespace vigilantune
