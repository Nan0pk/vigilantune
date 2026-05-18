# Changelog

All notable changes to this project will be documented in this file.

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
- **High-Fidelity Telemetry**: Added per-core frequency monitoring and thermal limit detection (Gap #4).
- **Timer Resolution Detection**: Added monitoring for system timer resolution pollution (Gap #8).
- **Power Request Protection**: Implemented Win32 Power Requests to protect AI inference cadence from system throttling (Gap #7).
- **Integration Testing**: Added `test_pipeline.cpp` to verify the full sensor-to-actuator control loop.
- **Inference Unit Tests**: Added `test_inference.cpp` for SHA-256 verification and model loading logic.
- **Sensor Unit Tests**: Added `test_sensors.cpp` for lifecycle and metric collection validation.

### Fixed
- **Critical #1 (C01/C03)**: Added strict NTSTATUS checking and RAII-style cleanup for BCrypt hashing in `InferenceManager`.
- **Critical #2 (C02)**: Resolved race condition in thread shutdown logic using atomic exchange.
- **Critical #4 (C04)**: Ensured PowerSetActiveScheme correctly persists changes by using GUIDs directly from the OS.
- **Critical #5 (C05)**: Fixed use-after-free in `SensorManager` WinEvent callbacks using atomic reference counting.
- **Security #2 (S02)**: Added input sanitization for all PDH counter values to prevent NaN propagation.
- **Build #2 (B02)**: Improved ONNX Runtime detection in CMake and added WSPA_DISABLE_AI guards for builds without ONNX.
- **Build #4 (B04)**: Disabled data collection by default for alpha builds to prevent unintentional logging.

### Changed
- **Improved Fallback Controller**: Replaced hardcoded thresholds with a linear interpolation model for smoother power adjustments when AI is disabled.
- **SSS Weight Rebalancing**: Adjusted Stress Score weights to accommodate new GPU and Disk metrics.
