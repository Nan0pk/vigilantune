# Windows SCADA Power Agent (WSPA)

An intelligent, low-overhead background optimization agent designed for Windows. Operating like an industrial Supervisory Control and Data Acquisition (SCADA) system, it dynamically balances system responsiveness and battery longevity by intercepting OS events, evaluating state telemetry via an embedded ONNX machine learning model, and tuning hardware power registers via native Win32 APIs.

---

## 🏛️ System Architecture

The architecture decouples telemetry ingestion (Sensors) from system tuning (Actuators) using an in-memory, thread-safe Real-Time Tag Database. A deterministic control loop drives the AI inference engine, wrapped securely inside an independent system watchdog.

```
[ Windows Kernel / Win32 ]
     │      ▲
     │      │ (1. Event Hooks / 2. Coalesced Timers)
     ▼      │
[ 📊 TAG DATABASE ] <─── Gives Snapshot ─── [ 🧠 ONNX Model ] (Runs every 100ms)
     │                                              │
     │ (Calculates Stress Score)                    │ (Outputs relative changes)
     ▼                                              ▼
[ ⚙️ ACTUATOR LAYER ] <── Filtered by Deadband ─────┘
     │
     ▼ (Applies Safe GUID Updates)
[ Windows Power APIs ]
```

---

## 🛠️ Core Functional Components

### 1. Sensor & Telemetry Layer (Hybrid Lanes)

To achieve a near-zero idle resource footprint, WSPA replaces standard polling loops with a specialized dual-lane ingestion pipeline:

* **The Interrupt Lane (Event-Driven):** Uses the native Win32 `SetWinEventHook` API to listen for `EVENT_SYSTEM_FOREGROUND`. The thread sleeps at 0% CPU usage until the OS signals a window state change, immediately updating the `Foreground_App` tag.
* **The Coalesced Lane (Time-Series Data):** Captures continuous performance metrics (e.g., Thread Queue Length, Utilization, System Wattage) using `SetCoalescableTimer`. By introducing a 10ms tolerance window, Windows batches these sensor wakeups with existing OS timers, maximizing the duration the processor remains in deep sleep states ($C\text{-states}$).

### 2. Controller Layer (The AI Loop)

* **Deterministic Evaluation:** The embedded ONNX runtime processes system snapshots at a fixed 100ms interval, ensuring the controller maintains a steady execution cadence and does not overreact to transient micro-spikes.
* **Dirty Flag Gatekeeper:** Prior to running a full machine learning inference pass, the agent computes a fast hash of the current Tag Database rows. If the hardware state is identical to the previous 100ms cycle (e.g., the user is reading a static document), the ONNX execution block is bypassed entirely to prevent unnecessary CPU cycles.

### 3. Actuator Layer (Adaptive Deadband)

The actuator layer takes the relative adjustments output by the ONNX model and applies them to Windows Power GUIDs. To avoid excessive API write thrashing, it uses a **System Stress Score (SSS)** to drive an **Adaptive Deadband**:

$$\text{SSS} = f(\text{CPU\_Run\_Queue}, \text{Thermal\_Headroom}, \text{Core\_Utilization})$$

The `CPU_Run_Queue` depth acts as a leading performance indicator and carries exponential weight.

| System Stress Score (SSS) | System State | Deadband Width | Actuator Behavior |
| --- | --- | --- | --- |
| **High (70–100)** 🚨 | Heavy thread backlog / Low thermal headroom | **0% – 1%** (Ultra-sensitive) | Applies every micro-adjustment instantly to mitigate interface latency. |
| **Medium (30–69)** ⚖️ | Standard active workflow (Web browsing, editing) | **2% – 5%** (Balanced) | Filters background noise; executes meaningful power shifts. |
| **Low (0–29)** 💤 | Idle desktop / Static reading | **6% – 10%** (Wide) | Ignores minor shifts to maintain low-power $C\text{-states}$. |

### 4. Safety & Recovery Layer (The Watchdog)

To guarantee system stability if the user-space agent crashes, an independent, lightweight Watchdog service (`watchdog.exe`) runs concurrently.

* **Zero-Overhead Monitoring:** The Watchdog opens a handle to the main agent process and immediately blocks execution using `WaitForSingleObject(hAgent, INFINITE)`. The kernel removes the Watchdog thread from the active scheduling queue, costing 0% CPU.
* **Fail-Safe Escalation:** The moment the main agent terminates, the Watchdog thread is awakened by a hardware interrupt. It evaluates the process exit code using `GetExitCodeProcess`:
* If the exit code is `0` (Clean, intentional user shutdown), it closes gracefully.
* If the exit code is non-zero (Unexpected crash), the Watchdog instantly resets the Windows Power Scheme to default via `powercfg /setactive SCHEME_BALANCED`, increments a recovery counter, and attempts a clean restart up to 3 times before logging a critical error and exiting.

---

## 📂 Project Repository Structure

```text
├── .devcontainer/     # Production environment container configurations
├── src/
│   ├── agent/          # Main process: Sensor lanes, Tag DB, and ONNX engine
│   ├── watchdog/       # Standalone process supervisor (WaitForSingleObject loop)
│   └── shared/         # Common Win32 P/Invoke signatures and data structures
├── models/             # Compiled ONNX execution graphs
├── .gitignore          # Pre-configured to strip MSVC build artifacts (.obj, .exe, .pdb)
└── CMakeLists.txt      # Root CMake configuration targeting the MSVC toolchain
```

---

## 🚀 Getting Started

### 📦 Prerequisites

* Windows 10 / 11 (x64 or ARM64)
* Git
* CMake (v3.20+)
* Visual Studio Build Tools (MSVC Compiler)

### 🔨 Compilation & Build Pipeline

You can orchestrate your local environment setup or run commands using your terminal agent:

1. Clone the repository:
```bash
git clone https://github.com/geminipro123pakistan-ctrl/WinSCADA.git
cd WinSCADA
```

2. Generate the build files via CMake and compile:

```bash
   mkdir build
   cd build
   cmake ..
   cmake --build . --config Release
```

3. The compiled binaries (`agent.exe` and `watchdog.exe`) will be generated inside the `build/Release/` directory.

---

## 📜 License

This project is licensed under the MIT License - see the LICENSE file for details.
