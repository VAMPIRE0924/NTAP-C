# NTAP-C

Linux TAP client with AUTH_TAP relay mode, automatic CONFIG_PUSH direct-first strategy, direct-only token TAP mode, direct-first relay fallback, and Windows TAP-Windows6 backend scaffolding.

This repository is exported from the NTAP integration workspace. Keep git
history source-only: do not commit build output, runtime databases, logs, or
generated release archives. Final release packages belong in GitHub Releases.

## Build

    make
    make config-test

## Layout

    src/common/  shared protocol and helpers
    src/c/  component source
    conf/        minimal example config
    scripts/windows/  Windows TAP-Windows6 smoke and driver preflight helper

## Windows TAP

The Windows data-plane backend supports TAP-Windows6/OpenVPN style adapters.
Wintun/WireGuard adapters are detected by check-env but need a later backend.

Run the smoke helper from PowerShell. Without -RequireTap it skips cleanly when
no TAP-Windows6 adapter is installed:

    powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\windows\smoke-ntap-c-tap.ps1 -Build

Use -RequireTap on a real Windows TAP validation host:

    powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\windows\smoke-ntap-c-tap.ps1 -Build -RequireTap