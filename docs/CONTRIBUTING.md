# Contributing to WinSCADA

Thank you for your interest in improving Windows system efficiency! As an alpha-stage project, your contributions—whether they are code, bug reports, or telemetry data—are vital.

## 📊 Telemetry Contributions (Most Helpful!)
The future of WinSCADA depends on a high-quality dataset. If you have been running the agent in `DATA_COLLECTION_MODE`, you can help by sharing your log file.

### How to share telemetry safely:
1. Locate your `models/telemetry_log.csv`.
2. **Review your data**: The log contains numeric system metrics (CPU%, GPU%, etc.) and a **numeric hash** of your foreground application titles. It does **not** store the actual window titles or any personal information.
3. Open a [Telemetry Contribution Issue](https://github.com/Nan0pk/vigilantune/issues/new?title=Telemetry+Data+Contribution) and attach your CSV file.

## 💻 Code Contributions
1. Fork the repository.
2. Create a feature branch (`git checkout -b feature/amazing-sensor`).
3. **Run the Tests**: Ensure all `ctest` cases pass.
4. Commit your changes.
5. Push to the branch and open a Pull Request.

### Code Style
- Follow the C++20 standard.
- Use the established `wspa` namespace.
- Ensure all new sensors use the `TagDatabase` with proper synchronization.

## 🐛 Bug Reports
If you find a hardware configuration where WinSCADA fails:
- State your CPU, GPU, and Windows Version.
- Include any error messages from the console (e.g., "[Sensors] PDH Counter failed").
- Explain what you were doing when the failure occurred.

---
**WinSCADA Security Policy**: Please do not report security vulnerabilities in public issues. Instead, email the maintainers directly or use the GitHub Private Vulnerability Reporting feature.
