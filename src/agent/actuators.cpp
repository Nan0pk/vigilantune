#include "actuators.hpp"
#include <cmath>
#include <initguid.h>
#include <algorithm>
#include <vector>
#include <powrprof.h>
#include "../shared/config.hpp"
#include "../shared/scoped_handle.hpp"
#include "../shared/logger.hpp"

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
            LOG_INFO("Actuator", "Custom WinSCADA scheme not found. Creating from Balanced base...");
            GUID* scheme_ptr = nullptr;
            if (PowerDuplicateScheme(NULL, &GUID_TYPICAL_POWER_SAVINGS, &scheme_ptr) == ERROR_SUCCESS) {
                ScopedLocalPtr<GUID> guard(scheme_ptr);
                m_active_scheme = *guard;
                
                std::wstring name = L"WinSCADA AI Optimized";
                PowerWriteFriendlyName(NULL, &m_active_scheme, NULL, NULL, (UCHAR*)name.c_str(), (DWORD)(name.length() * 2 + 2));
            } else {
                LOG_ERROR("Actuator", "Failed to duplicate scheme. Falling back to Active.");
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
        ScopedLocalPtr<GUID> guard(active_guid);
        if (result == ERROR_SUCCESS && guard) {
            m_active_scheme = *guard;
        } else {
            LOG_ERROR("Actuator", "Failed to query active power scheme. Error: " << result);
        }
    }

    void ActuatorManager::queue_adjustment(ActuatorID param, double value) {
        m_queued_adjustments.set(param, value);
    }

    void ActuatorManager::queue_adjustments(const ActuationSet& adjustments) {
        for (size_t i = 0; i < static_cast<size_t>(ActuatorID::MAX_ACTUATORS); ++i) {
            if (adjustments.has_value[i]) {
                m_queued_adjustments.set(static_cast<ActuatorID>(i), adjustments.values[i]);
            }
        }
    }

    bool ActuatorManager::commit_changes(double stress_score) {
        if (m_queued_adjustments.empty()) return true;

        // Implementation #4: Refresh before write to ensure target is valid
        refresh_active_scheme();

        double deadband = calculate_deadband(stress_score);
        bool any_change = false;

        for (size_t i = 0; i < static_cast<size_t>(ActuatorID::MAX_ACTUATORS); ++i) {
            if (!m_queued_adjustments.has_value[i]) continue;

            ActuatorID param = static_cast<ActuatorID>(i);
            double value = m_queued_adjustments.values[i];

            if (should_apply(param, value, deadband)) {
                bool success = false;
                const GUID* setting_guid = nullptr;
                
                if (param == ActuatorID::PerformanceBoost) {
                    setting_guid = &GUID_PROCESSOR_THROTTLE_MAX;
                    LOG_INFO("Actuator", "Applying PerformanceBoost = " << value 
                              << " (Deadband: " << deadband << "%)");
                } else if (param == ActuatorID::ProcessorFloor) {
                    setting_guid = &GUID_PROCESSOR_THROTTLE_MIN;
                    LOG_INFO("Actuator", "Applying ProcessorFloor = " << value 
                              << " (Deadband: " << deadband << "%)");
                }

                if (setting_guid) {
                    DWORD dwValue = (DWORD)std::clamp(value, 0.0, 100.0);
                    
                    DWORD ac_res = PowerWriteACValueIndex(NULL, &m_active_scheme, &GUID_PROCESSOR_SETTINGS_SUBGROUP, setting_guid, dwValue);
                    DWORD dc_res = PowerWriteDCValueIndex(NULL, &m_active_scheme, &GUID_PROCESSOR_SETTINGS_SUBGROUP, setting_guid, dwValue);
                    
                    if (ac_res != ERROR_SUCCESS) LOG_ERROR("Actuator", "AC Write failed. Error: " << ac_res);
                    if (dc_res != ERROR_SUCCESS) LOG_ERROR("Actuator", "DC Write failed. Error: " << dc_res);
                    
                    success = (ac_res == ERROR_SUCCESS && dc_res == ERROR_SUCCESS);
                }

                if (success) {
                    m_last_applied_values.set(param, value);
                    any_change = true;
                }
            }
        }

        m_queued_adjustments.has_value.fill(false); // Reset queue

        if (any_change) {
            // Fix for Issue C04: Use the GUID returned from PowerGetActiveScheme 
            // instead of a local copy to ensure the OS correctly processes the re-activation.
            GUID* active_guid = nullptr;
            DWORD res = PowerGetActiveScheme(NULL, &active_guid);
            ScopedLocalPtr<GUID> guard(active_guid);
            if (res == ERROR_SUCCESS && guard) {
                res = PowerSetActiveScheme(NULL, guard.get());
            }

            if (res != ERROR_SUCCESS) {
                LOG_ERROR("Actuator", "Failed to re-activate scheme. Error: " << res);
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

    bool ActuatorManager::should_apply(ActuatorID param, double new_value, double deadband) {
        size_t idx = static_cast<size_t>(param);
        if (!m_last_applied_values.has_value[idx]) {
            return true;
        }

        double last_value = m_last_applied_values.values[idx];
        double diff = std::abs(new_value - last_value);
        
        return diff >= deadband;
    }

    bool ActuatorManager::set_active_scheme(const GUID* scheme_guid) {
        DWORD result = PowerSetActiveScheme(NULL, scheme_guid);
        if (result != ERROR_SUCCESS) {
            LOG_ERROR("Actuator", "Failed to set active power scheme. Error: " << result);
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
