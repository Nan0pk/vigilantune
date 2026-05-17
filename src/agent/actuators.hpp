#pragma once
#include <windows.h>
#include <powersetting.h>
#include <string>
#include <map>

namespace wspa {
    class ActuatorManager {
    public:
        ActuatorManager();
        ~ActuatorManager();

        // High-level interface for AI adjustments
        void apply_adjustment(const std::string& param, double value, double stress_score);

        // Low-level Win32 Power API wrappers
        bool set_active_scheme(const GUID* scheme_guid);
        bool update_setting(const GUID* sub_group_guid, const GUID* setting_guid, DWORD value);

    private:
        double calculate_deadband(double stress_score);
        bool should_apply(const std::string& param, double new_value, double deadband);

        GUID m_active_scheme;
        std::map<std::string, double> m_last_applied_values;
    };
}
