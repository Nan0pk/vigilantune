# 🚀 WinSCADA v0.1.0-alpha: Developer Preview

Welcome to the first public preview of the **Windows SCADA Power Agent (WSPA)**.

### ⚠️ IMPORTANT: PROTOTYPE STATUS
This is an **Alpha-stage developer preview**. WinSCADA interacts with low-level Windows Power GUIDs and kernel-level performance counters. 
- **Intended Audience**: C++ developers, AI enthusiasts, and power-users.
- **Risk**: While hardened with a safety watchdog, there is a non-zero risk of system instability on untested hardware. Use at your own risk.

---

## 🎯 Primary Goal: Data Collection
The "AI" in this release is currently in its infancy. The main purpose of `v0.1.0-alpha` is to **crowd-source telemetry** to build a high-fidelity dataset for the future WinSCADA MLP (Multi-Layer Perceptron) model.

By running the agent, you help us understand how different CPUs, GPUs, and Thermal zones behave under real-world Windows workloads.

---

## ✨ Features in this Preview
- **Dual-Lane Telemetry**: Event-driven foreground app tracking + coalesced PDH metrics (CPU, GPU, Disk, Queue, Thermal).
- **Hardened Tag Database**: Thread-safe, RW-locked central storage for all system state data.
- **Safety Watchdog**: Zero-overhead process supervisor that resets the system to the "Balanced" power scheme on agent failure.
- **Adaptive Deadband Actuator**: Intelligently filters power register writes to prevent kernel thrashing.
- **SHA-256 Verification**: Secure model loading with BCrypt-based checksum validation.
- **Integrated Training Pipeline**: Complete PyTorch suite for offline model generation.

---

## 🛠️ How You Can Help
1. **Build and Run**: Follow the instructions in the `README.md`.
2. **Collect Data**: Ensure `DATA_COLLECTION_MODE = true` in `config.hpp`.
3. **Share Your Logs**: Contribute your `models/telemetry_log.csv` to our training pool (see `CONTRIBUTING.md`).

## 📦 Prerequisites
- Windows 10/11 (x64 or ARM64)
- CMake 3.20+
- Visual Studio 2022 (MSVC)
- [Optional] ONNX Runtime

---
**Full Changelog**: [v0.0.1...v0.1.0-alpha](https://github.com/geminipro123pakistan-ctrl/WinSCADA/compare/main)
