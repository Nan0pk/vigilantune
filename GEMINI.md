# Project Instructions: Windows SCADA Power Agent (WSPA)

This file contains foundational mandates for the WSPA project. Adhere to these principles to maintain system integrity and performance.

> [!IMPORTANT]
> **Source of Truth:** This local environment is the foundational context. However, the online repository represents the final, authoritative shape of the project. Always prioritize the online/remote implementation if discrepancies are found.

## 🏛️ Architecture & Principles
- **Industrial Determinism:** Treat the system like a SCADA controller. Use a decoupled Sensor/Actuator model.
- **Zero-Allocation Hot Path:** The core control loop, telemetry ingestion, and actuator writes MUST be zero-allocation. Use `std::array` and `std::atomic` instead of heap-allocated containers.
- **Lock-Free Communication:** Communication between telemetry lanes and the control loop must be lock-free via the Real-Time Tag Database.
- **Win32 Native First:** Prioritize native Win32 APIs (PDH, Power APIs, BCrypt, WinEvents) over cross-platform abstractions to ensure minimum overhead.

## 🛠️ Implementation Guidelines
- **Telemetry:** Distinguish between "Interrupt Lane" (WinEvents) and "Coalesced Lane" (PDH).
- **Control Loop:** Interval MUST scale (50ms to 500ms) based on Stress Score (SSS).
- **Safety:** Every modification MUST be supervised by the Watchdog. A `FAILSAFE_SCHEME_GUID` must always be ready for fallback.
- **Validation:** Performance benchmarks are first-class citizens. Always verify changes against the `test_benchmark.cpp` suite.

## 📦 Dependencies
- **ONNX Runtime:** Used for inference. Verify model SHA-256 before loading.
- **GTest:** Primary testing framework.
- **Vcpkg:** Recommended for dependency management.

## 🧪 Testing Standards
- All new features must include unit tests in the `tests/` directory.
- Behavioral correctness and performance impact must be verified before merging.
