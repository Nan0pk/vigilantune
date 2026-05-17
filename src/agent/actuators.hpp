#pragma once
#include <windows.h>
#include <powrprof.h>
#include <map>
#include <string>

namespace wspa {
    class ActuatorManager {
    public:
        ActuatorManager();
        ~ActuatorManager();

        // High-level method to apply adjustments with adaptive deadband
        void apply_adjustment(const std::string& param, double value, double stress_score);

        bool set_active_scheme(const GUID* scheme_guid);
        bool update_setting(const GUID* scheme_guid, const GUID* sub_group_guid, const GUID* setting_guid, DWORD value);

    private:
        double calculate_deadband(double stress_score);
        bool should_apply(const std::string& param, double new_value, double deadband);

        std::map<std::string, double> m_last_applied_values;
        
        // Active scheme cache
        GUID m_active_scheme;
    };
}
