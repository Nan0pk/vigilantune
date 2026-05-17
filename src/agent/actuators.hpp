#pragma once
#include <windows.h>
#include <powrprof.h>
#include <vector>

namespace wspa {
    class ActuatorManager {
    public:
        ActuatorManager();
        ~ActuatorManager();

        // Sets the active power scheme (e.g., Balanced, Power Saver, High Performance)
        bool set_active_scheme(const GUID* scheme_guid);

        // Adjusts a specific power setting within a scheme
        bool update_setting(const GUID* scheme_guid, const GUID* sub_group_guid, const GUID* setting_guid, DWORD value);

    private:
        // Helper to get the current active scheme
        GUID* get_current_scheme();
    };
}
