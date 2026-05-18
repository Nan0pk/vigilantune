#include "actuators.hpp"
#include <iostream>
#include <cmath>
#include <initguid.h>
#include <algorithm>
#include <vector>
#include <powrprof.h>
#include "../shared/config.hpp"

#pragma comment(lib, "PowrProf.lib")

// Processor Settings Subgroup
DEFINE_GUID(GUID_PROCESSOR_SETTINGS_SUBGROUP, 0x54533251, 0x82be, 0x4824, 0x96, 0xc1, 0x47, 0xb6, 0x0b, 0x74, 0x0d, 0x00);
// Maximum Processor State
DEFINE_GUID(GUID_PROCESSOR_THROTTLE_MAX, 0xbcbb0383, 0x0504, 0x42db, 0x9a, 0x3c, 0x90, 0x43, 0xe0, 0x33, 0x8d, 0xd1);
// Minimum Processor State
DEFINE_GUID(GUID_PROCESSOR_THROTTLE_MIN, 0x893dee03, 0x5242, 0x4642, 0xbe, 0x5d, 0x0a, 0x7c, 0x4a, 0xf6, 0x43, 0xd4);

// WinSCADA Custom Scheme GUID
// {7FBDC34D-3932-414F-8E34-4C5E095A6E4B}
DEFINE_GUID(GUID_WINSCADA_SCHEME, 0x7fbdc34d, 0x3932, 0x414f, 0x8e, 0x34, 0x4c, 0x5e, 0x09, 0x5a, 0x6e, 0x4b);

namespace wspa {
    ActuatorManager::ActuatorManager() {
        // Architecture #4: Create or find custom WinSCADA scheme by name
        bool found = false;
        GUID scheme_guid = { 0 };
        DWORD buffer_size = sizeof(GUID);
        for (DWORD i = 0; PowerEnumerate(NULL, NULL, NULL, ACCESS_SCHEME, i, (UCHAR*)&scheme_guid, &buffer_size) == ERROR_SUCCESS; ++i) {
            DWORD name_size = 0;
            PowerReadFriendlyName(NULL, &scheme_guid, NULL, NULL, NULL, &name_size);
            if (name_size > 0) {
                std::vector<UCHAR> buffer(name_size);
                if (PowerReadFriendlyName(NULL, &scheme_guid, NULL, NULL, buffer.data(), &name_size) == ERROR_SUCCESS) {
                    std::wstring name((wchar_t*)buffer.data());
                    if (name == L"WinSCADA AI Optimized") {
                        m_active_scheme = scheme_guid;
                        found = true;
                        break;
                    }
                }
            }
            buffer_size = sizeof(GUID);
        }

        if (!found) {
            std::cout << "[Actuator] Custom WinSCADA scheme not found. Creating from Balanced base..." << std::endl;
            GUID* scheme_ptr = nullptr;
            if (PowerDuplicateScheme(NULL, &GUID_TYPICAL_POWER_SAVINGS, &scheme_ptr) == ERROR_SUCCESS) {
                m_active_scheme = *scheme_ptr;
                LocalFree(scheme_ptr);
                
                std::wstring name = L"WinSCADA AI Optimized";
                PowerWriteFriendlyName(NULL, &m_active_scheme, NULL, NULL, (UCHAR*)name.c_str(), (DWORD)(name.length() * 2 + 2));
            } else {
                std::cerr << "[Actuator] Failed to duplicate scheme. Falling back to Active." << std::endl;
                refresh_active_scheme();
            }
        }

        // Force activation of our scheme
        PowerSetActiveScheme(NULL, &m_active_scheme);
    }

    ActuatorManager::~ActuatorManager() {}

    void ActuatorManager::refresh_active_scheme() {
        GUID* active_guid = nullptr;
        DWORD result = PowerGetActiveScheme(NULL, &active_guid);
        if (result == ERROR_SUCCESS && active_guid) {
            m_active_scheme = *active_guid;
            LocalFree(active_guid);
        } else {
            std::cerr << "[Actuator] Failed to query active power scheme. Error: " << result << std::endl;
        }
    }

    void ActuatorManager::queue_adjustment(const std::string& param, double value) {
        m_queued_adjustments[param] = value;
    }

    bool ActuatorManager::commit_changes(double stress_score) {
        if (m_queued_adjustments.empty()) return true;

        // Implementation #4: Refresh before write to ensure target is valid
        refresh_active_scheme();

        double deadband = calculate_deadband(stress_score);
        bool any_change = false;

        for (const auto& [param, value] : m_queued_adjustments) {
            if (should_apply(param, value, deadband)) {
                std::cout << "[Actuator] Applying " << param << " = " << value 
                          << " (Deadband: " << deadband << "%)" << std::endl;
                
                bool success = false;
                const GUID* setting_guid = nullptr;
                
                if (param == "PerformanceBoost") {
                    setting_guid = &GUID_PROCESSOR_THROTTLE_MAX;
                } else if (param == "ProcessorFloor") {
                    setting_guid = &GUID_PROCESSOR_THROTTLE_MIN;
                }

                if (setting_guid) {
                    DWORD dwValue = (DWORD)std::clamp(value, 0.0, 100.0);
                    
                    DWORD ac_res = PowerWriteACValueIndex(NULL, &m_active_scheme, &GUID_PROCESSOR_SETTINGS_SUBGROUP, setting_guid, dwValue);
                    DWORD dc_res = PowerWriteDCValueIndex(NULL, &m_active_scheme, &GUID_PROCESSOR_SETTINGS_SUBGROUP, setting_guid, dwValue);
                    
                    if (ac_res != ERROR_SUCCESS) std::cerr << "[Actuator] AC Write failed for " << param << ". Error: " << ac_res << std::endl;
                    if (dc_res != ERROR_SUCCESS) std::cerr << "[Actuator] DC Write failed for " << param << ". Error: " << dc_res << std::endl;
                    
                    success = (ac_res == ERROR_SUCCESS && dc_res == ERROR_SUCCESS);
                }

                if (success) {
                    m_last_applied_values[param] = value;
                    any_change = true;
                }
            }
        }

        m_queued_adjustments.clear();

        if (any_change) {
            // Fix for Issue C04: Use the GUID returned from PowerGetActiveScheme 
            // instead of a local copy to ensure the OS correctly processes the re-activation.
            GUID* active_guid = nullptr;
            DWORD res = PowerGetActiveScheme(NULL, &active_guid);
            if (res == ERROR_SUCCESS && active_guid) {
                res = PowerSetActiveScheme(NULL, active_guid);
                LocalFree(active_guid);
            }

            if (res != ERROR_SUCCESS) {
                std::cerr << "[Actuator] Failed to re-activate scheme. Error: " << res << std::endl;
                return false;
            }
        }

        return true;
    }

    double ActuatorManager::calculate_deadband(double stress_score) {
        if (stress_score >= 70.0) return config::DEADBAND_HIGH_STRESS;
        if (stress_score >= 30.0) return config::DEADBAND_MEDIUM_STRESS;
        return config::DEADBAND_LOW_STRESS;
    }

    bool ActuatorManager::should_apply(const std::string& param, double new_value, double deadband) {
        if (m_last_applied_values.find(param) == m_last_applied_values.end()) {
            return true;
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
        DWORD ac_res = PowerWriteACValueIndex(NULL, &m_active_scheme, sub_group_guid, setting_guid, value);
        DWORD dc_res = PowerWriteDCValueIndex(NULL, &m_active_scheme, sub_group_guid, setting_guid, value);
        
        if (ac_res != ERROR_SUCCESS || dc_res != ERROR_SUCCESS) {
             return false;
        }
        return set_active_scheme(&m_active_scheme);
    }
}
