# NTAP-C

Linux TAP client with AUTH_TAP relay mode plus direct-only token TAP mode.

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