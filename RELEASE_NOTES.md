# 🚀 nanoloop v0.3.0-beta: Project Rebranding & Version Bump

Welcome to **nanoloop v0.3.0-beta**! This release marks the full rebranding of the project from **WinSCADA / WSPA** to **nanoloop**, namespace updates, version bumps, and various stability improvements.

### ⚠️ IMPORTANT: PROTOTYPE STATUS
This is a **Beta-stage developer preview**. nanoloop interacts with low-level Windows Power GUIDs and kernel-level performance counters. 
- **Intended Audience**: C++ developers, AI enthusiasts, and power-users.
- **Risk**: While hardened with a safety watchdog, there is a non-zero risk of system instability on untested hardware. Use at your own risk.

---

## ✨ Features in this Version
- **Full Project Rebrand**: Rebranded all components and files from **WinSCADA / WSPA** to **nanoloop**.
- **Namespace Migration**: Fully migrated all codebases and unit tests from `wspa` namespace to the modern `nanoloop` namespace.
- **IPC Cohesion**: Renamed named Windows IPC heartbeat references from `Local\WinSCADA_Heartbeat` to `Local\Nanoloop_Heartbeat`.
- **Power Scheme Alignment**: Updated the custom Windows power scheme name to `"Nanoloop AI Optimized"` and assigned an updated GUID scheme.
- **Improved Uninstallation Flow**: Enhanced `uninstall.ps1` to cleanly remove both old and new power schemes and stop running processes.
- **Hardened Safety Watchdog**: Multi-stage recovery with exponential backoff and fail-safe power reset. Includes resume-from-sleep cooldown robustness.
- **SHA-256 Integrity Verification**: Hardened BCrypt-based model validation.
- **Power Request Protection**: Kernel-level protection for AI inference cadence.
- **Integrated Training Pipeline**: Complete PyTorch suite for offline model generation.

---

## 🛠️ How You Can Help
1. **Build and Run**: Follow the instructions in the `README.md`.
2. **Collect Data**: Ensure `data_collection = true` in `nanoloop_config.ini` to collect telemetry logs.
3. **Share Your Logs**: Contribute your `models/telemetry_log.csv` to our training pool (see `CONTRIBUTING.md`).

## 📦 Prerequisites
- Windows 10/11 (x64)
- CMake 3.20+
- Visual Studio 2022 (MSVC)
- [Optional] ONNX Runtime

---
**Full Changelog**: [v0.2.0-beta...v0.3.0-beta](https://github.com/geminipro123pakistan-ctrl/nanoloop/compare/main)
