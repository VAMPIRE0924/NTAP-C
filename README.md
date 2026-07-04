# NTAP-C 

NTAP-C is the NTAP client side. On Windows it is customer-facing: double-click `ntap-c.exe`, enter connection information, click Connect, and the GUI writes local config, checks TAP, and prepares the TAP adapter with administrator permission when needed.

Linux keeps a command-line entry point for server, test, and automation use.

## Repository Set

NTAP is split into three clean source repositories. Deployable packages are published only through each repository's GitHub Releases.

- [NTAP-A](https://github.com/VAMPIRE0924/NTAP-A): public server, management API, SQLite state, node/TAP authentication, and TapHub relay.
- [NTAP-B](https://github.com/VAMPIRE0924/NTAP-B): node side, installed at the customer gateway or internal host, connects to A, and joins the local network.
- [NTAP-C](https://github.com/VAMPIRE0924/NTAP-C): client side, with a Windows GUI for customers and a Linux command-line entry point.

## Download And Deploy

Use the final packages from GitHub Releases. Do not deploy temporary files from a source checkout.

Latest release:

https://github.com/VAMPIRE0924/NTAP-C/releases/latest

Windows customer package:

```text
NTAP-C-<version>-windows-x64.zip
```

After extraction, customers run:

```text
bin\ntap-c.exe
```

The window asks for:

- NTAP-A address
- TAP username
- TAP password
- Network ID
- TAP adapter name, default `ntap-c0`

After Connect, the GUI calls the bundled `ntap-c-cli.exe` for the actual connection. `ntap-c-cli.exe` is for validation, automation, and service-style usage, not for normal customers.

## Windows TAP

The current Windows data path supports TAP-Windows6/OpenVPN-style adapters. Wintun/WireGuard adapters are detected but need a separate data path backend later.

If the customer machine has no TAP-Windows6 adapter, the GUI tries to call this release-package helper:

```text
install\ensure-tap-windows.ps1
```

The helper needs administrator permission. It can create or prepare the TAP adapter when supported OpenVPN TAP tools or driver files are available. If no driver exists, install the OpenVPN TAP-Windows6 driver first.

## Linux Client

Linux client package:

```text
NTAP-C-<version>-linux-x64.tar.gz
```

Basic commands from the release package:

```sh
bin/ntap-c -c conf/ntap-c.conf.example check-env
bin/ntap-c -c conf/ntap-c.conf.example run
```

Linux needs `/dev/net/tun` and permission to create TAP devices.

## Source Scope

```text
src/c/       NTAP-C client source
src/common/  shared protocol and utility source
conf/        minimal example config
```

This repository keeps only source code, example config, README, and LICENSE. Final deployable packages live in GitHub Releases.

## License

GPL-3.0-only. See `LICENSE`.
