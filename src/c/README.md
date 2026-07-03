# src/c

NTAP-C TAP terminal client.

Current implemented scope:

```text
AUTH_TAP
CONFIG_PUSH consumption before TAP setup
Linux TAP open/read/write relay path
real netns ping smoke against NTAP-A + NTAP-B
direct-only token mode against NTAP-B direct listener
direct-first fallback to AUTH_TAP relay when direct setup fails
automatic direct-first strategy consumption from CONFIG_PUSH v2
`ntap-c check-env` Linux TAP privilege preflight and Windows TAP/Wintun adapter discovery
Windows TAP-Windows6/OpenVPN adapter open/read/write data-plane backend
Windows GUI executable for customer use, with CLI retained as ntap-c-cli.exe
```

On Windows, `ntap-c.exe` is the customer-facing GUI: it prompts for connection
values, writes a local config, checks TAP, and can elevate the packaged TAP
preparation helper. `ntap-c-cli.exe` is the automation and validation entry.

Windows TAP-Windows6 data-plane code still needs validation on a machine with a
TAP-Windows6 adapter installed. Wintun/WireGuard adapters are detected by
`check-env` but require a later Wintun-specific backend.

Run `scripts/windows/smoke-ntap-c-tap.ps1 -Build` to preflight the Windows
driver path. Add `-RequireTap` on a host that must have TAP-Windows6 installed.
