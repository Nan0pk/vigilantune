# Security Policy

## Supported Versions

| Version        | Supported          |
| -------------- | ------------------ |
| v0.3.0-beta    | :white_check_mark: |
| v0.2.0-beta    | :x:                |

## Reporting a Vulnerability

If you discover a security vulnerability in nanoloop Power Agent, please report it responsibly:

1. **Email:** Send a detailed report to the project maintainer via the email listed in the repository profile.
2. **GitHub Private Advisory:** Open a [private security advisory](https://docs.github.com/en/code-security/security-advisories/working-with-repository-security-advisories/creating-a-repository-security-advisory) on the nanoloop GitHub repository.

Please include the following in your report:

- A clear description of the vulnerability and its potential impact.
- Steps to reproduce the issue.
- Any suggested mitigations or patches.

We will acknowledge receipt within **48 hours** and aim to provide a resolution timeline within **7 days**.

> **Note:** Please do **not** open public issues for security vulnerabilities.

## Security Controls in Place

The following security measures are implemented in the current release:

- **SHA-256 Model Integrity Verification (BCrypt):** The ONNX inference model is cryptographically verified using the Win32 BCrypt API before loading to prevent tampering or supply-chain substitution.
- **Input Sanitization for PDH Counter Values (NaN Protection):** All Performance Data Helper counter readings are sanitized to reject NaN and infinite values before entering the control loop, preventing undefined floating-point propagation.
- **Atomic Reference Counting for Use-After-Free Prevention:** The WinEvent sensor callback system uses atomic reference counting to guarantee safe lifetime management and prevent use-after-free conditions during shutdown.
- **Fail-Safe Power Scheme Assertion on Crash:** The independent watchdog process immediately asserts a known-safe `FAILSAFE_SCHEME_GUID` power scheme upon any agent crash, preventing the system from remaining in an aggressive or unknown power state.
- **Windows Manifests for Elevation Requirements:** Application manifests declare the required execution level, ensuring the agent and watchdog only run with the correct privileges.

## Known Limitations

The following security limitations are known and accepted for the current beta release:

- **No Code Signing (Authenticode):** Release binaries are not currently signed with an Authenticode certificate. Users should verify downloads via the SHA-256 checksums published in the GitHub release assets.
- **ASLR/DEP/CFG Rely on MSVC Defaults:** Address Space Layout Randomization, Data Execution Prevention, and Control Flow Guard are enabled via MSVC compiler and linker defaults (`/DYNAMICBASE`, `/NXCOMPAT`, `/guard:cf`). No additional hardening flags are explicitly configured beyond the toolchain defaults.
