# Changelog

All notable changes to this project will be documented in this file.

## [0.3.0-beta] - 2026-05-21

### Added
- **Project Rename**: Fully rebranded the project from **WinSCADA / WSPA** to **nanoloop**.
- **Namespace Migration**: Migrated all C++ namespace declarations and imports from `wspa` to `nanoloop`.
- **IPC Namespace Protection**: Renamed the lock-free shared memory heartbeat IPC channel from `Local\WinSCADA_Heartbeat` to `Local\Nanoloop_Heartbeat` to ensure synchronization and prevent conflicts.
- **Power Scheme Rebranding**: Rebranded the custom Windows power scheme to `"Nanoloop AI Optimized"` and updated its GUID (`GUID_NANOLOOP_SCHEME`).
- **Compile-time Definitions**: Replaced compile-time AI toggles and logs (`WSPA_DISABLE_AI`, `WSPA_LOG_LEVEL`) with rebranded `NANOLOOP_DISABLE_AI` and `NANOLOOP_LOG_LEVEL` flags.
- **Improved Uninstallation Script**: Updated `uninstall.ps1` to stop running processes and cleanly delete both old `WinSCADA AI Optimized` and new `Nanoloop AI Optimized` power schemes from the host OS.
- **Version Bump**: Bumped the project version to `0.3.0` across vcpkg manifests, assembly manifests, and documentation.

## [0.2.0-beta] - 2026-05-18

### Changed
- **Lock-Free Tag Database**: Completely rewrote the TagDatabase to eliminate `std::shared_mutex` and `std::unordered_map`. Replaced with a fully lock-free, zero-allocation `std::array` of `std::atomic<double>`. Telemetry ingestion latency reduced to ~14.8 ns/op.
- **Zero-Allocation Control Loop**: Eliminated all heap churn (`std::vector`, `std::map`) in the AI Control and Actuator pipelines. The `Controller::evaluate` hot path now executes in ~24 ns/op.
- **Governor Decoupling**: Moved the $O(N)$ `ProcessGovernor` system scan into a dedicated background thread to preserve the deterministic 50ms cadence of the main AI control loop.
- **Watchdog Hardening**: Refined the Watchdog to operate as an elite industrial safety layer. It now asserts the `FAILSAFE_SCHEME_GUID` immediately upon agent crash and uses strict exponential backoff for recovery attempts, terminating after max retries to prevent cyclic damage.
- **Telemetry Hash Inlining**: `Foreground_App` is now hashed immediately within the WinEvent callback, completely removing `std::string` dependencies from the shared telemetry bus.

### Added
- **Performance Benchmarks**: Added `test_benchmark.cpp` to continuously validate ultra-low-latency and zero-allocation constraints via GTest.

## [0.1.0-alpha] - 2026-05-18

### Added
- **GPU & Disk Integration**: Stress Score (SSS) now accounts for GPU utilization and Physical Disk pressure.
- **High-Fidelity Telemetry**: Added per-core frequency monitoring and thermal limit detection.
- **Timer Resolution Detection**: Added monitoring for system timer resolution pollution.
- **Power Request Protection**: Implemented Win32 Power Requests to protect AI inference cadence from system throttling.
- **Integration Testing**: Added `test_pipeline.cpp` to verify the full sensor-to-actuator control loop.
- **Inference Unit Tests**: Added `test_inference.cpp` for SHA-256 verification and model loading logic.
- **Sensor Unit Tests**: Added `test_sensors.cpp` for lifecycle and metric collection validation.

### Fixed
- **Critical Checking**: Added strict NTSTATUS checking and RAII-style cleanup for BCrypt hashing in `InferenceManager`.
- **Race Condition Resolution**: Resolved race condition in thread shutdown logic using atomic exchange.
- **Power Scheme Persistence**: Ensured PowerSetActiveScheme correctly persists changes by using GUIDs directly from the OS.
- **Use-After-Free Prevention**: Fixed use-after-free in `SensorManager` WinEvent callbacks using atomic reference counting.
- **Input Sanitization**: Added input sanitization for all PDH counter values to prevent NaN propagation.
- **Build Guards**: Improved ONNX Runtime detection in CMake and added `NANOLOOP_DISABLE_AI` guards for builds without ONNX.
- **Hardened Defaults**: Disabled data collection by default for alpha builds to prevent unintentional logging.

### Changed
- **Improved Fallback Controller**: Replaced hardcoded thresholds with a linear interpolation model for smoother power adjustments when AI is disabled.
- **SSS Weight Rebalancing**: Adjusted Stress Score weights to accommodate new GPU and Disk metrics.
