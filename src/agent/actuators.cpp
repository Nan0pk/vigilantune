#include "actuators.hpp"
#include <iostream>
#include <cmath>

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
            
            // In a real implementation, map 'param' to a specific Power GUID
            // and call update_setting()
            
            m_last_applied_values[param] = value;
        } else {
            // Filtered by deadband
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
        
        // Check if change exceeds the deadband threshold
        return diff >= deadband;
    }

    bool ActuatorManager::set_active_scheme(const GUID* scheme_guid) {
        DWORD result = PowerSetActiveScheme(NULL, scheme_guid);
        if (result != ERROR_SUCCESS) {
            std::cerr << "[Actuator] Failed to set active power scheme. Error: " << result << std::endl;
            return false;
        }
        m_active_scheme = *scheme_guid;
        return true;
    }

    bool ActuatorManager::update_setting(const GUID* scheme_guid, const GUID* sub_group_guid, const GUID* setting_guid, DWORD value) {
        DWORD result = PowerWriteACValueIndex(NULL, scheme_guid, sub_group_guid, setting_guid, value);
        if (result != ERROR_SUCCESS) return false;
        return set_active_scheme(scheme_guid);
    }
}
