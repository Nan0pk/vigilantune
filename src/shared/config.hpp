#pragma once
#include <string>
#include <vector>
#include <cmath>
#include <windows.h>
#include "config_loader.hpp"

namespace nanoloop {
    namespace config {
        // Paths (Defaults)
        inline std::wstring MODEL_PATH = L"models/power_model.onnx";
        inline std::string TELEMETRY_LOG_PATH = "models/telemetry_log.csv";
        
        // Security: Model Verification (SHA-256)
        // Set this to the hash output of models/train.py
        inline std::string EXPECTED_MODEL_HASH = "ca00e30d9d8ab99689f1798e39e3d280a8685cf37617c2a9078cfa628dcc9c56"; 
        
        // Mode
        inline bool DATA_COLLECTION_MODE = false; 
        
        // Intervals (ms)
        inline int TELEMETRY_INTERVAL_MS = 1000;
        inline int MIN_CONTROL_LOOP_INTERVAL_MS = 50;
        inline int MAX_CONTROL_LOOP_INTERVAL_MS = 500;
        
        // Power Scheme GUIDs
        // Balanced: 381b4222-f694-41f0-9685-ff5bb260df2e
        inline GUID FAILSAFE_SCHEME_GUID = { 0x381b4222, 0xf694, 0x41f0, { 0x96, 0x85, 0xff, 0x5b, 0xb2, 0x60, 0xdf, 0x2e } };
        
        // Hashing & Sensitivity
        inline int APP_HASH_BUCKETS = 1000;
        inline double DIRTY_FLAG_EPSILON = 1.0; // 1.0% change threshold
        
        // Deadband sensitivity thresholds
        inline double DEADBAND_HIGH_STRESS = 1.0;
        inline double DEADBAND_MEDIUM_STRESS = 5.0;
        inline double DEADBAND_LOW_STRESS = 10.0;
        
        // Stress Score weights
        inline double SSS_CPU_WEIGHT = 0.30;
        inline double SSS_QUEUE_WEIGHT = 0.40;
        inline double SSS_THERMAL_WEIGHT = 0.10;
        inline double SSS_GPU_WEIGHT = 0.10;
        inline double SSS_DISK_WEIGHT = 0.10;
 
        // Governor settings
        inline int GOVERNOR_INTERVAL_MS = 5000;
        inline std::vector<std::string> EXCLUSION_LIST = { "csrss.exe", "dwm.exe", "explorer.exe", "svchost.exe" };
 
        // Watchdog settings
        inline int MAX_RECOVERY_ATTEMPTS = 3;
 
        // Telemetry Rotation settings
        inline int MAX_FILE_SIZE_MB = 50;
        inline int MAX_ROTATED_FILES = 5;
 
        // Load config from nanoloop_config.ini
        inline bool load_from_file(const std::string& filepath) {
            ConfigLoader loader;
            if (!loader.load(filepath)) {
                return false;
            }
 
            DATA_COLLECTION_MODE = loader.get_bool("general.data_collection", DATA_COLLECTION_MODE);
            
            std::string model_path_str = loader.get_string("general.model_path", "");
            if (!model_path_str.empty()) {
                MODEL_PATH = std::wstring(model_path_str.begin(), model_path_str.end());
            }
            
            TELEMETRY_LOG_PATH = loader.get_string("general.telemetry_log_path", TELEMETRY_LOG_PATH);
 
            // [controller]
            MIN_CONTROL_LOOP_INTERVAL_MS = loader.get_int("controller.min_control_loop_interval_ms", MIN_CONTROL_LOOP_INTERVAL_MS);
            MAX_CONTROL_LOOP_INTERVAL_MS = loader.get_int("controller.max_control_loop_interval_ms", MAX_CONTROL_LOOP_INTERVAL_MS);
            TELEMETRY_INTERVAL_MS = loader.get_int("controller.telemetry_interval_ms", TELEMETRY_INTERVAL_MS);
            DIRTY_FLAG_EPSILON = loader.get_double("controller.dirty_flag_epsilon", DIRTY_FLAG_EPSILON);
 
            // [deadband]
            DEADBAND_HIGH_STRESS = loader.get_double("deadband.high_stress", DEADBAND_HIGH_STRESS);
            DEADBAND_MEDIUM_STRESS = loader.get_double("deadband.medium_stress", DEADBAND_MEDIUM_STRESS);
            DEADBAND_LOW_STRESS = loader.get_double("deadband.low_stress", DEADBAND_LOW_STRESS);
 
            // [stress_weights]
            double w_cpu = loader.get_double("stress_weights.cpu", SSS_CPU_WEIGHT);
            double w_queue = loader.get_double("stress_weights.queue", SSS_QUEUE_WEIGHT);
            double w_thermal = loader.get_double("stress_weights.thermal", SSS_THERMAL_WEIGHT);
            double w_gpu = loader.get_double("stress_weights.gpu", SSS_GPU_WEIGHT);
            double w_disk = loader.get_double("stress_weights.disk", SSS_DISK_WEIGHT);
            // Verify sum is approx 1.0
            if (std::abs((w_cpu + w_queue + w_thermal + w_gpu + w_disk) - 1.0) < 0.01) {
                SSS_CPU_WEIGHT = w_cpu;
                SSS_QUEUE_WEIGHT = w_queue;
                SSS_THERMAL_WEIGHT = w_thermal;
                SSS_GPU_WEIGHT = w_gpu;
                SSS_DISK_WEIGHT = w_disk;
            }
 
            // [governor]
            auto list = loader.get_list("governor.exclusion_list");
            if (!list.empty()) {
                EXCLUSION_LIST = list;
            }
            GOVERNOR_INTERVAL_MS = loader.get_int("governor.governor_interval_ms", GOVERNOR_INTERVAL_MS);
 
            // [watchdog]
            MAX_RECOVERY_ATTEMPTS = loader.get_int("watchdog.max_recovery_attempts", MAX_RECOVERY_ATTEMPTS);
 
            // [telemetry_rotation]
            MAX_FILE_SIZE_MB = loader.get_int("telemetry_rotation.max_file_size_mb", MAX_FILE_SIZE_MB);
            MAX_ROTATED_FILES = loader.get_int("telemetry_rotation.max_rotated_files", MAX_ROTATED_FILES);
 
            return true;
        }
    }
}
