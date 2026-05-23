#pragma once
#include <windows.h>
#include <string>

namespace vigilantune {

    struct GpuMetrics {
        double temperature_c{0.0};
        double power_mw{0.0};
        bool is_valid{false};
    };

    class GpuTelemetryLoader {
    public:
        GpuTelemetryLoader();
        ~GpuTelemetryLoader();

        GpuMetrics get_metrics();

        // Returns a human-readable string identifying the active GPU vendor
        const char* get_vendor_name() const;

    private:
        bool try_init_nvml();
        bool try_init_adl();
        bool try_init_intel();

        // Common
        HMODULE m_hMod{nullptr};
        HMODULE m_hModIntel{nullptr}; // gdi32.dll handle (always loaded, no FreeLibrary)
        int m_vendor{0}; // 0 = None, 1 = NVIDIA, 2 = AMD, 3 = Intel

        // NVIDIA NVML
        void* m_nvmlDevice{nullptr};
        typedef int (*nvmlInit_t)();
        typedef int (*nvmlShutdown_t)();
        typedef int (*nvmlDeviceGetHandleByIndex_t)(unsigned int, void**);
        typedef int (*nvmlDeviceGetTemperature_t)(void*, int, unsigned int*);
        typedef int (*nvmlDeviceGetPowerUsage_t)(void*, unsigned int*);
        
        nvmlDeviceGetTemperature_t m_nvmlGetTemp{nullptr};
        nvmlDeviceGetPowerUsage_t m_nvmlGetPower{nullptr};
        nvmlShutdown_t m_nvmlShutdown{nullptr};

        // AMD ADL
        typedef void* (__stdcall *ADL_MAIN_MALLOC_CALLBACK)(int);
        typedef int (*ADL_Main_Control_Create_t)(ADL_MAIN_MALLOC_CALLBACK, int);
        typedef int (*ADL_Main_Control_Destroy_t)();
        typedef int (*ADL_Adapter_NumberOfAdapters_Get_t)(int*);
        
        struct ADLTemperature {
            int iSize;
            int iTemperature;
        };
        typedef int (*ADL_Overdrive5_Temperature_Get_t)(int, int, ADLTemperature*);

        ADL_Overdrive5_Temperature_Get_t m_adlGetTemp{nullptr};
        ADL_Main_Control_Destroy_t m_adlDestroy{nullptr};
        int m_adlAdapterIndex{-1};

        // Intel iGPU via D3DKMT (gdi32.dll)
        // D3DKMT structures (minimal subset needed for temperature query)
        struct D3DKMT_OPENADAPTERFROMGDIDISPLAYNAME {
            WCHAR DeviceName[32];
            UINT hAdapter;
            LUID AdapterLuid;
            UINT VidPnSourceId;
        };

        // D3DKMT_QUERYADAPTERINFO structures for temperature
        struct D3DKMT_QUERYADAPTERINFO {
            UINT hAdapter;
            UINT Type;
            void* pPrivateDriverData;
            UINT PrivateDriverDataSize;
        };

        typedef LONG (APIENTRY *PFN_D3DKMTOpenAdapterFromGdiDisplayName)(D3DKMT_OPENADAPTERFROMGDIDISPLAYNAME*);
        typedef LONG (APIENTRY *PFN_D3DKMTCloseAdapter)(const UINT*);

        PFN_D3DKMTOpenAdapterFromGdiDisplayName m_d3dkmtOpenAdapter{nullptr};
        PFN_D3DKMTCloseAdapter m_d3dkmtCloseAdapter{nullptr};
        UINT m_intelAdapterHandle{0};

        // Intel temperature via WMI/PDH fallback (simpler, more portable)
        double query_intel_temperature();
    };

} // namespace vigilantune
