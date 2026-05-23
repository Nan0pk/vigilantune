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

// Energy Performance Preference (EPP)
DEFINE_GUID(GUID_PROCESSOR_PERF_ENERGY_PREFERENCE, 0x0d551686, 0x4f63, 0x45ec, 0x85, 0x5b, 0x9b, 0x19, 0x78, 0x43, 0xb0, 0x4c);
// System Cooling Policy (0 = Passive, 1 = Active)
DEFINE_GUID(GUID_SYSTEM_COOLING_POLICY, 0x94d3a615, 0xa899, 0x4ac5, 0xae, 0x2b, 0xe0, 0xd8, 0x90, 0x5a, 0x8d, 0x3a);

namespace vigilantune {
    ActuatorManager::ActuatorManager() {
        // Unhide required processor settings
        PowerWriteSettingAttributes(&GUID_PROCESSOR_SETTINGS_SUBGROUP, &GUID_PROCESSOR_THROTTLE_MAX, 0);
        PowerWriteSettingAttributes(&GUID_PROCESSOR_SETTINGS_SUBGROUP, &GUID_PROCESSOR_THROTTLE_MIN, 0);
        PowerWriteSettingAttributes(&GUID_PROCESSOR_SETTINGS_SUBGROUP, &GUID_PROCESSOR_PERF_ENERGY_PREFERENCE, 0);
        PowerWriteSettingAttributes(&GUID_PROCESSOR_SETTINGS_SUBGROUP, &GUID_SYSTEM_COOLING_POLICY, 0);
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
                    if (name == L"VigilanTune AI Optimized" || name == L"Nanoloop AI Optimized") {
                        m_active_scheme = scheme_guid;
                        found = true;
                        break;
                    }
                }
            }
            buffer_size = sizeof(GUID);
        }

        if (!found) {
            LOG_INFO("Actuator", "Custom VigilanTune scheme not found. Creating from Balanced base...");
            GUID* scheme_ptr = nullptr;
            if (PowerDuplicateScheme(NULL, &GUID_TYPICAL_POWER_SAVINGS, &scheme_ptr) == ERROR_SUCCESS) {
                nanoloop::ScopedLocalPtr<GUID> guard(scheme_ptr);
                m_active_scheme = *guard;
                
                std::wstring name = L"VigilanTune AI Optimized";
                PowerWriteFriendlyName(NULL, &m_active_scheme, NULL, NULL, (UCHAR*)name.c_str(), (DWORD)(name.length() * 2 + 2));
            } else {
                LOG_ERROR("Actuator", "Failed to duplicate scheme. Falling back to Active.");
                refresh_active_scheme();
            }
        }

        PowerSetActiveScheme(NULL, &m_active_scheme);
    }

    ActuatorManager::~ActuatorManager() {}

    void ActuatorManager::refresh_active_scheme() {
        GUID* active_guid = nullptr;
        DWORD result = PowerGetActiveScheme(NULL, &active_guid);
        nanoloop::ScopedLocalPtr<GUID> guard(active_guid);
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

    typedef NTSTATUS (NTAPI *NtSetTimerResolutionPtr)(ULONG DesiredResolution, BOOLEAN SetResolution, PULONG CurrentResolution);

    bool ActuatorManager::commit_changes(double stress_score) {
        if (m_queued_adjustments.empty()) return true;

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
                const GUID* subgroup_guid = &GUID_PROCESSOR_SETTINGS_SUBGROUP;
                
                if (param == ActuatorID::PerformanceBoost) {
                    setting_guid = &GUID_PROCESSOR_THROTTLE_MAX;
                    LOG_INFO("Actuator", "Applying PerformanceBoost = " << value << "% (Deadband: " << deadband << "%)");
                } else if (param == ActuatorID::ProcessorFloor) {
                    setting_guid = &GUID_PROCESSOR_THROTTLE_MIN;
                    LOG_INFO("Actuator", "Applying ProcessorFloor = " << value << "% (Deadband: " << deadband << "%)");
                } else if (param == ActuatorID::EnergyPreference) {
                    setting_guid = &GUID_PROCESSOR_PERF_ENERGY_PREFERENCE;
                    LOG_INFO("Actuator", "Applying EnergyPreference (EPP) = " << value << "% (Deadband: " << deadband << "%)");
                } else if (param == ActuatorID::SystemCooling) {
                    setting_guid = &GUID_SYSTEM_COOLING_POLICY;
                    LOG_INFO("Actuator", "Applying SystemCooling policy = " << (value > 0.5 ? "Active" : "Passive"));
                } else if (param == ActuatorID::TimerCadence) {
                    static NtSetTimerResolutionPtr NtSetTimerResolution = (NtSetTimerResolutionPtr)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtSetTimerResolution");
                    if (NtSetTimerResolution) {
                        ULONG currentRes;
                        ULONG desiredRes = (ULONG)(value * 10000.0);
                        if (NtSetTimerResolution(desiredRes, TRUE, &currentRes) == 0) {
                            LOG_INFO("Actuator", "Applying OS Timer Cadence = " << value << "ms (" << currentRes << " 100ns)");
                            success = true;
                        }
                    }
                }

                if (setting_guid && param != ActuatorID::TimerCadence) {
                    DWORD dwValue = (DWORD)std::clamp(value, 0.0, 100.0);
                    
                    DWORD ac_res = PowerWriteACValueIndex(NULL, &m_active_scheme, subgroup_guid, setting_guid, dwValue);
                    DWORD dc_res = PowerWriteDCValueIndex(NULL, &m_active_scheme, subgroup_guid, setting_guid, dwValue);
                    
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
            GUID* active_guid = nullptr;
            DWORD res = PowerGetActiveScheme(NULL, &active_guid);
            nanoloop::ScopedLocalPtr<GUID> guard(active_guid);
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
        if (stress_score >= 70.0) return nanoloop::config::DEADBAND_HIGH_STRESS;
        if (stress_score >= 30.0) return nanoloop::config::DEADBAND_MEDIUM_STRESS;
        return nanoloop::config::DEADBAND_LOW_STRESS;
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
} // namespace vigilantune
