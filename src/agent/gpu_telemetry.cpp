#include "gpu_telemetry.hpp"
#include "../shared/logger.hpp"
#include <dxgi.h>
#include <iostream>

#pragma comment(lib, "dxgi.lib")

namespace vigilantune {

    static void* __stdcall adl_malloc(int iSize) {
        return malloc(iSize);
    }

    GpuTelemetryLoader::GpuTelemetryLoader() {
        if (try_init_nvml()) {
            m_vendor = 1;
            LOG_INFO("Sensors", "NVIDIA NVML loaded successfully.");
        } else if (try_init_adl()) {
            m_vendor = 2;
            LOG_INFO("Sensors", "AMD ADL loaded successfully.");
        } else if (try_init_intel()) {
            m_vendor = 3;
            LOG_INFO("Sensors", "Intel iGPU adapter detected via D3DKMT/DXGI.");
        } else {
            LOG_WARN("Sensors", "No dedicated GPU telemetry DLL found. Relying on shared heatpipe extrapolation.");
        }
    }

    GpuTelemetryLoader::~GpuTelemetryLoader() {
        if (m_vendor == 1 && m_nvmlShutdown) {
            m_nvmlShutdown();
        } else if (m_vendor == 2 && m_adlDestroy) {
            m_adlDestroy();
        } else if (m_vendor == 3 && m_d3dkmtCloseAdapter && m_intelAdapterHandle != 0) {
            m_d3dkmtCloseAdapter(&m_intelAdapterHandle);
        }
        if (m_hMod) {
            FreeLibrary(m_hMod);
        }
        // m_hModIntel (gdi32.dll) is always loaded by the OS — never free it
    }

    const char* GpuTelemetryLoader::get_vendor_name() const {
        switch (m_vendor) {
            case 1: return "NVIDIA";
            case 2: return "AMD";
            case 3: return "Intel";
            default: return "None (Extrapolation)";
        }
    }

    bool GpuTelemetryLoader::try_init_nvml() {
        m_hMod = LoadLibraryA("nvml.dll");
        if (!m_hMod) return false;

        auto nvmlInit = (nvmlInit_t)GetProcAddress(m_hMod, "nvmlInit_v2");
        if (!nvmlInit) nvmlInit = (nvmlInit_t)GetProcAddress(m_hMod, "nvmlInit");

        m_nvmlShutdown = (nvmlShutdown_t)GetProcAddress(m_hMod, "nvmlShutdown");
        auto nvmlDeviceGetHandleByIndex = (nvmlDeviceGetHandleByIndex_t)GetProcAddress(m_hMod, "nvmlDeviceGetHandleByIndex_v2");
        if (!nvmlDeviceGetHandleByIndex) nvmlDeviceGetHandleByIndex = (nvmlDeviceGetHandleByIndex_t)GetProcAddress(m_hMod, "nvmlDeviceGetHandleByIndex");
        
        m_nvmlGetTemp = (nvmlDeviceGetTemperature_t)GetProcAddress(m_hMod, "nvmlDeviceGetTemperature");
        m_nvmlGetPower = (nvmlDeviceGetPowerUsage_t)GetProcAddress(m_hMod, "nvmlDeviceGetPowerUsage");

        if (nvmlInit && nvmlDeviceGetHandleByIndex && m_nvmlGetTemp) {
            if (nvmlInit() == 0) { // NVML_SUCCESS
                if (nvmlDeviceGetHandleByIndex(0, &m_nvmlDevice) == 0) {
                    return true;
                }
            }
        }

        FreeLibrary(m_hMod);
        m_hMod = nullptr;
        return false;
    }

    bool GpuTelemetryLoader::try_init_adl() {
        m_hMod = LoadLibraryA("atiadlxx.dll");
        if (!m_hMod) {
            m_hMod = LoadLibraryA("atiadlxy.dll");
        }
        if (!m_hMod) return false;

        auto adlCreate = (ADL_Main_Control_Create_t)GetProcAddress(m_hMod, "ADL_Main_Control_Create");
        m_adlDestroy = (ADL_Main_Control_Destroy_t)GetProcAddress(m_hMod, "ADL_Main_Control_Destroy");
        auto adlGetAdapters = (ADL_Adapter_NumberOfAdapters_Get_t)GetProcAddress(m_hMod, "ADL_Adapter_NumberOfAdapters_Get");
        m_adlGetTemp = (ADL_Overdrive5_Temperature_Get_t)GetProcAddress(m_hMod, "ADL_Overdrive5_Temperature_Get");

        if (adlCreate && adlGetAdapters && m_adlGetTemp) {
            if (adlCreate(adl_malloc, 1) == 0) { // ADL_OK
                int numAdapters = 0;
                if (adlGetAdapters(&numAdapters) == 0 && numAdapters > 0) {
                    m_adlAdapterIndex = 0; // Default to first adapter
                    return true;
                }
            }
        }

        FreeLibrary(m_hMod);
        m_hMod = nullptr;
        return false;
    }

    bool GpuTelemetryLoader::try_init_intel() {
        // Intel iGPUs (UHD, Iris, Arc) don't ship a standalone telemetry DLL.
        // We use D3DKMT from gdi32.dll to open the adapter, then query DXGI
        // for the adapter description to confirm it's Intel.
        
        m_hModIntel = GetModuleHandleA("gdi32.dll");
        if (!m_hModIntel) {
            m_hModIntel = LoadLibraryA("gdi32.dll");
        }
        if (!m_hModIntel) return false;

        m_d3dkmtOpenAdapter = (PFN_D3DKMTOpenAdapterFromGdiDisplayName)
            GetProcAddress(m_hModIntel, "D3DKMTOpenAdapterFromGdiDisplayName");
        m_d3dkmtCloseAdapter = (PFN_D3DKMTCloseAdapter)
            GetProcAddress(m_hModIntel, "D3DKMTCloseAdapter");

        if (!m_d3dkmtOpenAdapter) return false;

        // Enumerate DXGI adapters to find Intel
        IDXGIFactory* pFactory = nullptr;
        if (FAILED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&pFactory))) {
            return false;
        }

        IDXGIAdapter* pAdapter = nullptr;
        bool found_intel = false;
        for (UINT i = 0; pFactory->EnumAdapters(i, &pAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC desc;
            if (SUCCEEDED(pAdapter->GetDesc(&desc))) {
                // Intel vendor ID = 0x8086
                if (desc.VendorId == 0x8086) {
                    // Open the adapter via D3DKMT for later queries
                    D3DKMT_OPENADAPTERFROMGDIDISPLAYNAME openAdapter = {};
                    wcscpy_s(openAdapter.DeviceName, L"\\\\.\\DISPLAY1");
                    if (m_d3dkmtOpenAdapter(&openAdapter) == 0) {
                        m_intelAdapterHandle = openAdapter.hAdapter;
                        found_intel = true;
                    }
                    pAdapter->Release();
                    break;
                }
            }
            pAdapter->Release();
        }
        pFactory->Release();

        return found_intel;
    }

    double GpuTelemetryLoader::query_intel_temperature() {
        // Intel iGPUs don't expose temperature via a simple DLL like NVML.
        // The most reliable Windows-native path is the PDH "GPU" thermal zone
        // counter, which is already read by sensors.cpp in the coalesced lane.
        // For Intel Arc discrete GPUs, the igcl library (Intel Graphics Control
        // Library) can be used, but it requires a separate DLL that may not be
        // present on all systems. Here we return 0.0 to signal that the caller
        // should fall back to the shared-heatpipe extrapolation model.
        //
        // If the Intel Graphics Control Library (ControlLib.dll) is available,
        // a future enhancement can load it dynamically here.
        return 0.0;
    }

    GpuMetrics GpuTelemetryLoader::get_metrics() {
        GpuMetrics metrics;
        if (m_vendor == 1 && m_nvmlDevice) {
            unsigned int temp = 0;
            if (m_nvmlGetTemp(m_nvmlDevice, 0, &temp) == 0) {
                metrics.temperature_c = (double)temp;
                metrics.is_valid = true;
            }
            if (m_nvmlGetPower) {
                unsigned int power = 0;
                if (m_nvmlGetPower(m_nvmlDevice, &power) == 0) {
                    metrics.power_mw = (double)power;
                }
            }
        } else if (m_vendor == 2 && m_adlAdapterIndex >= 0) {
            ADLTemperature adlTemp;
            adlTemp.iSize = sizeof(ADLTemperature);
            if (m_adlGetTemp(m_adlAdapterIndex, 0, &adlTemp) == 0) {
                metrics.temperature_c = (double)(adlTemp.iTemperature / 1000.0);
                metrics.is_valid = true;
            }
            // AMD power usage via ADL requires OD6 or later, skipping for simplicity
        } else if (m_vendor == 3) {
            double intel_temp = query_intel_temperature();
            if (intel_temp > 0.0) {
                metrics.temperature_c = intel_temp;
                metrics.is_valid = true;
            }
            // Intel iGPU detected but temp not directly readable — caller uses
            // extrapolation. We still mark vendor=3 so the system knows an Intel
            // GPU exists and can factor GPU utilization from PDH counters.
        }
        return metrics;
    }

} // namespace vigilantune
