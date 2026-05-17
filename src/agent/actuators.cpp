#include "actuators.hpp"
#include <iostream>
#include <cmath>
#include <initguid.h>

// Processor Settings Subgroup
DEFINE_GUID(GUID_PROCESSOR_SETTINGS_SUBGROUP, 0x54533251, 0x82be, 0x4824, 0x96, 0xc1, 0x47, 0xb6, 0x0b, 0x74, 0x0d, 0x00);
// Maximum Processor State
DEFINE_GUID(GUID_PROCESSOR_THROTTLE_MAX, 0xbcbb0383, 0x0504, 0x42db, 0x9a, 0x3c, 0x90, 0x43, 0xe0, 0x33, 0x8d, 0xd1);

namespace wspa {
    ActuatorManager::ActuatorManager() {
        GUID* active_guid = nullptr;
        PowerGetActiveScheme(NULL, &active_guid);
        if (active_guid) {
            m_active_scheme = *active_guid;
            LocalFree(active_guid);
        }
    }

    ActuatorManager::~ActuatorManager() {}

    void ActuatorManager::apply_adjustment(const std::string& param, double value, double stress_score) {
        double deadband = calculate_deadband(stress_score);

        if (should_apply(param, value, deadband)) {
            std::cout << "[Actuator] Applying " << param << " = " << value 
                      << " (Deadband: " << deadband << "%)" << std::endl;
            
            bool success = false;
            if (param == "PerformanceBoost") {
                // Map 0-100 to Processor Throttle Max (0-100)
                DWORD dwValue = (DWORD)value;
                success = update_setting(&GUID_PROCESSOR_SETTINGS_SUBGROUP, &GUID_PROCESSOR_THROTTLE_MAX, dwValue);
            }

            if (success) {
                m_last_applied_values[param] = value;
            } else {
                std::cerr << "[Actuator] Failed to apply " << param << std::endl;
            }
        }
    }

    double ActuatorManager::calculate_deadband(double stress_score) {
        if (stress_score >= 70.0) return 1.0;  // High Stress: Ultra-sensitive
        if (stress_score >= 30.0) return 5.0;  // Medium Stress: Balanced
        return 10.0;                          // Low Stress: Wide deadband (save cycles)
    }

    bool ActuatorManager::should_apply(const std::string& param, double new_value, double deadband) {
        if (m_last_applied_values.find(param) == m_last_applied_values.end()) {
            return true; // First time applying
        }

        double last_value = m_last_applied_values[param];
        double diff = std::abs(new_value - last_value);
        
        return diff >= deadband;
    }

    bool ActuatorManager::set_active_scheme(const GUID* scheme_guid) {
        DWORD result = PowerSetActiveScheme(NULL, scheme_guid);
        if (result != ERROR_SUCCESS) {
            std::cerr << "[Actuator] Failed to set active power scheme. Error: " << result << std::endl;
            return false;
        }
        return true;
    }

    bool ActuatorManager::update_setting(const GUID* sub_group_guid, const GUID* setting_guid, DWORD value) {
        // Write both AC and DC value indexes
        DWORD ac_res = PowerWriteACValueIndex(NULL, &m_active_scheme, sub_group_guid, setting_guid, value);
        DWORD dc_res = PowerWriteDCValueIndex(NULL, &m_active_scheme, sub_group_guid, setting_guid, value);
        
        if (ac_res != ERROR_SUCCESS || dc_res != ERROR_SUCCESS) {
             std::cerr << "[Actuator] PowerWrite failed. AC: " << ac_res << ", DC: " << dc_res << std::endl;
             return false;
        }

        // Re-activating the scheme is necessary for the changes to take effect immediately
        return set_active_scheme(&m_active_scheme);
    }
}
