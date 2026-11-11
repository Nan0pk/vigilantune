#pragma once
#include "../src/agent/power_backend.hpp"

namespace vigilantune {
    class FakePowerBackend final : public IPowerBackend {
      public:
        FakePowerBackend(const GUID &, const std::wstring &) {}
        DWORD seed_setting(const GUID &, const GUID &, const GUID &, DWORD, DWORD) {
            return ERROR_CALL_NOT_IMPLEMENTED;
        }
        DWORD active_scheme(GUID &) override { return ERROR_CALL_NOT_IMPLEMENTED; }
        DWORD enumerate_schemes(std::vector<GUID> &) override { return ERROR_CALL_NOT_IMPLEMENTED; }
        DWORD friendly_name(const GUID &, std::wstring &) override { return ERROR_CALL_NOT_IMPLEMENTED; }
        DWORD duplicate_scheme(const GUID &, const GUID *, GUID &) override { return ERROR_CALL_NOT_IMPLEMENTED; }
        DWORD set_active_scheme(const GUID &) override { return ERROR_CALL_NOT_IMPLEMENTED; }
        DWORD delete_scheme(const GUID &) override { return ERROR_CALL_NOT_IMPLEMENTED; }
        DWORD set_friendly_name(const GUID &, const std::wstring &) override { return ERROR_CALL_NOT_IMPLEMENTED; }
        DWORD read_value(const GUID &, const GUID &, const GUID &, PowerSource, DWORD &) override {
            return ERROR_CALL_NOT_IMPLEMENTED;
        }
        DWORD write_value(const GUID &, const GUID &, const GUID &, PowerSource, DWORD) override {
            return ERROR_CALL_NOT_IMPLEMENTED;
        }
        DWORD enumerate_settings(const GUID &, const GUID &, std::vector<GUID> &) override {
            return ERROR_CALL_NOT_IMPLEMENTED;
        }
    };
} // namespace vigilantune
