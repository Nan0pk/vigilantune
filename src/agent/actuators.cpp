#include "actuators.hpp"
#include <iostream>

namespace wspa {
    ActuatorManager::ActuatorManager() {}
    ActuatorManager::~ActuatorManager() {}

    bool ActuatorManager::set_active_scheme(const GUID* scheme_guid) {
        DWORD result = PowerSetActiveScheme(NULL, scheme_guid);
        if (result != ERROR_SUCCESS) {
            std::cerr << "[Actuator] Failed to set active power scheme. Error: " << result << std::endl;
            return false;
        }
        std::cout << "[Actuator] Power scheme changed successfully." << std::endl;
        return true;
    }

    bool ActuatorManager::update_setting(const GUID* scheme_guid, const GUID* sub_group_guid, const GUID* setting_guid, DWORD value) {
        // Update AC value
        DWORD result = PowerWriteACValueIndex(NULL, scheme_guid, sub_group_guid, setting_guid, value);
        if (result != ERROR_SUCCESS) {
            std::cerr << "[Actuator] Failed to write AC value index. Error: " << result << std::endl;
            return false;
        }

        // Apply changes by re-setting the active scheme
        return set_active_scheme(scheme_guid);
    }

    GUID* ActuatorManager::get_current_scheme() {
        GUID* active_guid = nullptr;
        PowerGetActiveScheme(NULL, &active_guid);
        return active_guid;
    }
}
