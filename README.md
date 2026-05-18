# 🛡️ WinSCADA Power Agent 

An intelligent, ultra-low-latency background optimization agent designed for Windows. Operating like an industrial Supervisory Control and Data Acquisition (SCADA) system, it dynamically balances system responsiveness and battery longevity by intercepting OS events, evaluating state telemetry via an embedded ONNX machine learning model, and tuning hardware power registers via native Win32 APIs.

---

## 🏛️ System Architecture

The architecture is built on elite systems engineering principles: it completely decouples telemetry ingestion (Sensors) from system tuning (Actuators) using an in-memory, **lock-free, zero-allocation Real-Time Tag Database**. A highly deterministic AI-assisted control loop drives inference, wrapped securely inside an independent system watchdog.

```
[ Windows Kernel / Win32 ]
     │      ▲
     │      │ (1. WinEvent Hooks / 2. High-Fidelity PDH metrics)
     ▼      │
[ 📊 LOCK-FREE TAG DB ] <── Atomic Snapshot ── [ 🧠 ONNX Model ]
(std::array<atomic<double>>)                   (Zero-Allocation Pipeline)
     │                                              │
     │ (Calculates Stress Score)                    │ (Outputs batched changes)
     ▼                                              ▼
[ ⚙️ ACTUATOR LAYER ] <── Filtered by Deadband ─────┘
(Batched Writes)
     │
     ▼ (Applies Safe GUID Updates / Scheme Re-activation)
[ Windows Power APIs ]
```

### ⚡ Performance Benchmarks (v0.2.0)
WSPA is engineered for ultra-low latency and minimal scheduler wakeups. The core control path is completely zero-allocation (`std::array` + `std::atomic`).
- **Telemetry Ingestion (`TagDatabase::set`)**: ~14.8 ns/op
- **Dirty-State Evaluation (`Controller::evaluate`)**: ~24.0 ns/op
- **Mutex Contention**: 0 (Fully Lock-Free)
- **Heap Allocations in Hot Path**: 0

*Benchmarks recorded on Windows 11 (Intel P/E Hybrid Topology) running 1,000,000 continuous operations.*

---

## 🛠️ Core Functional Components

### 1. Sensor & Telemetry Layer (Hybrid Lanes)

* **The Interrupt Lane (Event-Driven):** Uses native Win32 `SetWinEventHook` to listen for foreground changes.
* **The Coalesced Lane (Time-Series Data):** Captures high-fidelity metrics via PDH.
    - **CPU/GPU/Disk:** Deep hardware utilization metrics.
    - **Thermal:** Auto-detects Kelvin/Celsius units to monitor `Thermal Zone Information`.
    - **Timer Pollution:** Monitors system-wide `NtQueryTimerResolution` requests.

### 2. Controller Layer (The AI Loop)

* **Deterministic Cadence:** The control loop interval scales gracefully between **50ms and 500ms** based on system stress (SSS), ensuring fast response during load and deep sleep during idle.
* **Security & Integrity:** The agent verifies the ONNX model's SHA-256 integrity using native Win32 BCrypt before loading.
* **Power Requests:** Protects inference cadence from kernel throttling using Win32 `PowerSetRequest`.

### 3. Actuator Layer (Zero-Allocation Pipeline)

* **Batched Writes:** Changes are queued in lock-free arrays and applied in a single batch to prevent kernel-level thrashing.
* **Adaptive Deadband:** Filters output jitter using multi-stage sensitivity thresholds.
* **Persistence:** Correctly negotiates scheme re-activation using strict GUID lifecycle management.

### 4. Safety & Recovery Layer (The Watchdog)

* **Industrial Safety Layer:** The isolated watchdog process immediately asserts a `FAILSAFE_SCHEME_GUID` upon any agent crash.
* **Exponential Backoff:** Ensures cyclic failure resilience before attempting system recovery.

---

## 🚀 Getting Started

### 📦 Prerequisites
* Windows 10 / 11 (x64/ARM64)
* CMake (v3.20+) & MSVC (VS 2022)
* [Optional] `vcpkg` for dependency automation.

### 🔨 Build & Validate
```powershell
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[path_to_vcpkg]/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

---

## 📜 License
MIT License.
