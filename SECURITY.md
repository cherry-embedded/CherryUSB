# Security Policy

## Supported versions

Security fixes are supported for CherryUSB releases starting with **1.7.0**, including the latest release.

| Version | Security fixes |
| --- | --- |
| 1.7.0 and later, including the latest release | Supported |
| Earlier than 1.7.0 | Not supported |

## Device-mode testing requirements

When testing USB device mode, the device application's descriptors must be correct.

## Reporting a vulnerability

Please report suspected security vulnerabilities privately:

- Use **Report a vulnerability** on the repository's [Security Advisories page](https://github.com/cherry-embedded/CherryUSB/security/advisories), when private vulnerability reporting is available.
- Alternatively, email **[1203593632@qq.com](mailto:1203593632@qq.com)** with the subject `CherryUSB security report: <short summary>`.

Please avoid posting details of an unpatched vulnerability in public issues or pull requests. For general bugs and support questions, use the repository's regular issue tracker.

Include the following information where possible:

- Affected release or commit, source files, and relevant functions.
- Whether the issue affects USB host mode, device mode, or both.
- MCU/SoC, USB controller and port driver, operating system, and relevant configuration macros.
- The input and conditions needed to trigger the issue, reproduction steps, and a minimal example or patch if available.
- Expected and observed behavior, potential impact, and any logs or traces.
- Your preferred name for attribution, or a request to remain anonymous.

## Handling and disclosure

Maintainers will assess the report and coordinate any fix and disclosure with the reporter. Affected versions, fixed versions, and any workarounds will be documented in the published advisory when confirmed.

## Published advisories

- [GitHub Security Advisories](https://github.com/cherry-embedded/CherryUSB/security/advisories)
- [Security advisory and CVE catalog](docs/en/security.rst)
- [安全公告与 CVE 目录（中文）](docs/zh/security.rst)
