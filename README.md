# 🛡️ Windows SCADA Power Agent (WSPA)

An intelligent, low-overhead background optimization agent designed for Windows. Operating like an industrial Supervisory Control and Data Acquisition (SCADA) system, it dynamically balances system responsiveness and battery longevity by intercepting OS events, evaluating state telemetry via an embedded ONNX machine learning model, and tuning hardware power registers via native Win32 APIs.

---

## 🏛️ System Architecture

The architecture decouples telemetry ingestion (Sensors) from system tuning (Actuators) using an in-memory, thread-safe Real-Time Tag Database. A deterministic control loop drives the AI inference engine, wrapped securely inside an independent system watchdog.

```
[ Windows Kernel / Win32 ]
     │      ▲
     │      │ (1. Event Hooks / 2. PDH Coalesced metrics)
     ▼      │
[ 📊 TAG DATABASE ] <─── Gives Snapshot ─── [ 🧠 ONNX Model ] (Runs every 100ms)
(std::shared_mutex)                         (MLP Optimizer)
     │                                              │
     │ (Calculates Stress Score)                    │ (Outputs batched changes)
     ▼                                              ▼
[ ⚙️ ACTUATOR LAYER ] <── Filtered by Deadband ─────┘
(Batched Writes)
     │
     ▼ (Applies Safe GUID Updates / Scheme Re-activation)
[ Windows Power APIs ]
```

---

## 🛠️ Core Functional Components

### 1. Sensor & Telemetry Layer (Hybrid Lanes)

To achieve a near-zero idle resource footprint, WSPA replaces standard polling loops with a specialized dual-lane ingestion pipeline:

* **The Interrupt Lane (Event-Driven):** Uses the native Win32 `SetWinEventHook` API to listen for `EVENT_SYSTEM_FOREGROUND`. The thread sleeps at 0% CPU usage until the OS signals a window state change, immediately updating the `Foreground_App` tag.
* **The Coalesced Lane (Time-Series Data):** Captures continuous performance metrics using the Windows **PDH (Performance Data Helper)** API.
    - **CPU & Disk:** Real-time utilization percentages.
    - **GPU:** Multi-engine aggregation using wildcard counter arrays.
    - **Thermal:** Auto-detects Kelvin/Celsius units to monitor `Thermal Zone Information`.
    - **Thread Queue:** Captured as a leading indicator of system latency.

### 2. Controller Layer (The AI Loop)

* **Thread-Safe Tag Database:** All telemetry is stored in a centralized `std::unordered_map` protected by `std::shared_mutex` (RW-locking), allowing high-frequency concurrent writes from sensors and consistent snapshots for the AI.
* **Deterministic Evaluation:** Snapshot processing occurs at a fixed 100ms interval.
* **Dirty Flag Gatekeeper:** Inference is bypassed if the hardware state hasn't changed beyond a **1.0% epsilon** threshold, minimizing idle CPU cycles.
* **App Hashing:** Foreground window titles are converted to stable **FNV-1a hashes**, allowing the ONNX model to recognize specific software suites without string overhead.

### 3. Actuator Layer (Adaptive Deadband)

The actuator layer takes the relative adjustments output by the ONNX model and applies them to Windows Power GUIDs (e.g., `GUID_PROCESSOR_THROTTLE_MAX`).

$$\text{SSS} = f(\text{CPU}, \text{Log}(\text{Queue}), \text{Thermal\_Pressure})$$

| System Stress Score (SSS) | System State | Deadband Width | Actuator Behavior |
| --- | --- | --- | --- |
| **High (70–100)** 🚨 | Heavy backlog / High heat | **0% – 1%** (Ultra-sensitive) | Immediate micro-adjustments for latency mitigation. |
| **Medium (30–69)** ⚖️ | Standard active workflow | **2% – 5%** (Balanced) | Filters noise; executes meaningful power shifts. |
| **Low (0–29)** 💤 | Idle / Static reading | **6% – 10%** (Wide) | Ignores minor shifts to maintain deep sleep states ($C\text{-states}$). |

**Optimization:** Changes are queued and applied in a **single batch**, re-activating the Windows Power Scheme exactly once per cycle to prevent kernel-level thrashing.

### 4. Safety & Recovery Layer (The Watchdog)

An independent `watchdog.exe` monitors the agent for maximum reliability.

* **Zero-Overhead Monitoring:** Uses `WaitForSingleObject` kernel primitive (0% CPU idle).
* **Fail-Safe Escalation:** Upon detecting an agent crash, the watchdog:
    1. Instantly resets the system to the **Balanced Power Scheme**.
    2. Attempts a clean restart of the agent (up to 3 times).
    3. Logs critical failures if manual intervention is required.

---

## 🧠 Offline Training Pipeline

WinSCADA includes a complete PyTorch-based pipeline to train custom power models.

1. **Collect Data:** Set `DATA_COLLECTION_MODE = true` in `src/shared/config.hpp`. The agent will log live telemetry to `models/telemetry_log.csv`.
2. **Train:** Install dependencies via `models/requirements.txt` and run `python models/train.py`.
3. **Export:** The script automatically exports a Multi-Layer Perceptron (MLP) to `models/power_model.onnx`.
4. **Deploy:** The agent will automatically detect and load the new model on the next launch.

---

## 📂 Project Repository Structure

```text
├── src/
│   ├── agent/          # Main process: Sensor lanes, Tag DB, and ONNX engine
│   ├── watchdog/       # Standalone process supervisor
│   └── shared/         # Centralized types, configurations, and Win32 helpers
├── models/             # PyTorch training suite and ONNX model binaries
├── monitor_power.ps1   # Portable PowerShell telemetry validator (CIM-based)
└── CMakeLists.txt      # Root CMake configuration targeting MSVC
```

---

## 🚀 Getting Started

### 📦 Prerequisites

* Windows 10 / 11 (x64 or ARM64)
* CMake (v3.20+)
* Visual Studio 2022 (MSVC Compiler)
* [Optional] ONNX Runtime (Detected automatically by CMake)

### 🔨 Build Instructions

1. Clone and build:
```powershell
mkdir build ; cd build
cmake ..
cmake --build . --config Release
```

*Note: If ONNX Runtime is not found, the build will automatically disable AI features and use the deterministic fallback logic.*

---

## 📜 License

This project is licensed under the MIT License.
