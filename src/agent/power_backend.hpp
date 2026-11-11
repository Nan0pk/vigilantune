#pragma once
#include <windows.h>
#include <string>
#include <vector>

namespace vigilantune {
    enum class PowerSource { AC, DC };

    class IPowerBackend {
      public:
        virtual ~IPowerBackend() = default;
        virtual DWORD active_scheme(GUID &scheme) = 0;
        virtual DWORD enumerate_schemes(std::vector<GUID> &schemes) = 0;
        virtual DWORD friendly_name(const GUID &scheme, std::wstring &name) = 0;
        virtual DWORD duplicate_scheme(const GUID &source, const GUID *destination, GUID &result) = 0;
        virtual DWORD set_active_scheme(const GUID &scheme) = 0;
        virtual DWORD delete_scheme(const GUID &scheme) = 0;
        virtual DWORD set_friendly_name(const GUID &scheme, const std::wstring &name) = 0;
        virtual DWORD read_value(const GUID &scheme, const GUID &subgroup, const GUID &setting, PowerSource source,
                                 DWORD &value) = 0;
        virtual DWORD write_value(const GUID &scheme, const GUID &subgroup, const GUID &setting, PowerSource source,
                                  DWORD value) = 0;
        virtual DWORD enumerate_settings(const GUID &scheme, const GUID &subgroup, std::vector<GUID> &settings) = 0;
    };
} // namespace vigilantune
