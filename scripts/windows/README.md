# scripts/windows

Windows deployment and TAP-Windows6 validation helpers for NTAP-C.

```text
install-ntap-c.ps1          Install a compiled NTAP-C Windows package, write config, run validation, and optionally start the client.
validate-tap-windows.ps1    Validate package contents, TAP-Windows6 adapter presence, and ntap-c check-env output.
deploy-remote.ps1           Copy a compiled NTAP-C Windows release zip to a reachable Windows TAP host over PowerShell Remoting, run install/validation, and fetch the report.
smoke-ntap-c-tap.ps1        Local build-time TAP smoke helper.
```

Use the remote helper after the target host has PowerShell Remoting enabled and
TAP-Windows6 installed. Keep the TAP password in an environment variable or a
file so it does not have to be typed into the command line:

```powershell
$env:NTAP_TAP_PASSWORD = '<tap-password>'
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\windows\deploy-remote.ps1 `
  -Version <version> `
  -Host <windows-tap-host> `
  -ServerAddr '<ntap-a-host>:8024' `
  -Username '<tap-user>' `
  -NetworkId 1 `
  -RequireTap
```

Use `-DryRun` to validate the generated PowerShell Remoting flow without
connecting. Use `-TargetDryRun` when the target is reachable but you want the
packaged installer to print intended changes without installing.
