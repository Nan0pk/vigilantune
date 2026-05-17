#pragma once
#include <string>
#include <windows.h>

namespace wspa {
    namespace config {
        // Paths
        inline const std::wstring MODEL_PATH = L"models/power_model.onnx";
        inline const std::string TELEMETRY_LOG_PATH = "models/telemetry_log.csv";
        
        // Security: Model Verification (SHA-256 placeholder or simple checksum)
        // In production, this would be a real hash of the trusted model
        inline const size_t EXPECTED_MODEL_SIZE = 0; // Set after training
        
        // Mode
        inline const bool DATA_COLLECTION_MODE = true; 
        
        // Intervals (ms)
        inline const int TELEMETRY_INTERVAL_MS = 1000;
        inline const int MIN_CONTROL_LOOP_INTERVAL_MS = 50;
        inline const int MAX_CONTROL_LOOP_INTERVAL_MS = 500;
        
        // Power Scheme GUIDs
        // Balanced: 381b4222-f694-41f0-9685-ff5bb260df2e
        inline const GUID FAILSAFE_SCHEME_GUID = { 0x381b4222, 0xf694, 0x41f0, { 0x96, 0x85, 0xff, 0x5b, 0xb2, 0x60, 0xdf, 0x2e } };
        
        // Hashing & Sensitivity
        inline const int APP_HASH_BUCKETS = 1000;
        inline const double DIRTY_FLAG_EPSILON = 1.0; // 1.0% change threshold
        
        // Deadband sensitivity thresholds
        inline const double DEADBAND_HIGH_STRESS = 1.0;
        inline const double DEADBAND_MEDIUM_STRESS = 5.0;
        inline const double DEADBAND_LOW_STRESS = 10.0;
        
        // Stress Score weights
        inline const double SSS_CPU_WEIGHT = 0.35;
        inline const double SSS_QUEUE_WEIGHT = 0.50;
        inline const double SSS_THERMAL_WEIGHT = 0.15;
    }
}
