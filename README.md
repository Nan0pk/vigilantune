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
[ 📊 TAG DATABASE ] <─── Gives Snapshot ─── [ 🧠 ONNX Model ] (Adaptive Interval)
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

* **The Interrupt Lane (Event-Driven):** Uses native Win32 `SetWinEventHook` to listen for foreground changes.
* **The Coalesced Lane (Time-Series Data):** Captures high-fidelity metrics via PDH.
    - **GPU:** Multi-engine aggregation using "Max Engine Proxy".
    - **Thermal:** Auto-detects Kelvin/Celsius units to monitor `Thermal Zone Information`.
    - **Thread Queue:** Logarithmic scaling to reflect exponential latency impact.

### 2. Controller Layer (The AI Loop)

* **Adaptive Evaluation:** The control loop interval scales between **50ms and 500ms** based on system stress (SSS), ensuring fast response during load and zero waste during idle.
* **Security & Integrity:** The agent verifies the ONNX model's size/integrity before loading.
* **Dirty Flag Gatekeeper:** Inference is bypassed if hardware state change is below the configured epsilon (default 1.0%).

### 3. Actuator Layer (Adaptive Deadband)

* **Batched Writes:** Changes are queued and applied in a single batch to prevent kernel-level thrashing.
* **Error Resilience:** Comprehensive logging and validation for all Power API calls.
* **Configurable Sensitivity:** All thresholds (Epsilon, Deadbands, Weights) are centralized in `config.hpp`.

### 4. Safety & Recovery Layer (The Watchdog)

* **Mutual Monitoring:** The agent alerts if the watchdog is missing, while the watchdog provides a zero-overhead kernel wait for agent health.
* **Fail-Safe Escalation:** Upon crash, resets to a configured safe-state scheme and attempts restart with **Exponential Backoff**.

---

## 🧠 Offline Training Pipeline

1. **Collect Data:** Set `DATA_COLLECTION_MODE = true` in `src/shared/config.hpp`.
2. **Train:** `pip install -r models/requirements.txt` -> `python models/train.py`.
3. **Deploy:** Agent verifies and loads the resulting `models/power_model.onnx`.

---

## 📂 Project Repository Structure

```text
├── src/
│   ├── agent/          # Main process: Sensors, Tag DB, and AI
│   ├── watchdog/       # Process supervisor with restart logic
│   └── shared/         # Config, types, and Win32 helpers
├── models/             # PyTorch suite and ONNX binaries
├── monitor_power.ps1   # Portable CIM-based telemetry validator
└── vcpkg.json          # Dependency management (ONNX Runtime)
```

---

## 🚀 Getting Started

### 📦 Prerequisites
* Windows 10 / 11 (x64/ARM64)
* CMake (v3.20+) & MSVC (VS 2022)
* [Optional] `vcpkg` for dependency automation.

### 🔨 Build Instructions
```powershell
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[path_to_vcpkg]/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

### 📊 Validation
Use `.\monitor_power.ps1` to independently verify CPU and battery trends while the agent is running.

---

## 📜 License
MIT License.
