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
- **Multi-Factor Stress Analysis**: Real-time analysis of CPU, GPU, Disk, and Thermal pressure.
- **Dual-Lane Telemetry**: Event-driven foreground app tracking + coalesced high-fidelity PDH metrics.
- **Hardened Tag Database**: Thread-safe, RW-locked central storage for all system state data.
- **Adaptive Fallback Controller**: Smooth linear power adjustments when AI is disabled.
- **Safety Watchdog**: Multi-stage recovery with exponential backoff and fail-safe power reset.
- **SHA-256 Integrity Verification**: Hardened BCrypt-based model validation.
- **Power Request Protection**: Kernel-level protection for AI inference cadence.
- **Integrated Training Pipeline**: Complete PyTorch suite for offline model generation.

---

## 🛠️ Recent Stability Fixes (Audit 2026-05-18)
This release includes critical fixes from our first repository audit:
- Fixed race conditions in shutdown and use-after-free in event hooks.
- Hardened model verification and added PDH input sanitization.
- Improved Windows Power Subsystem persistence logic.
- Added comprehensive unit and integration testing suite.

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
