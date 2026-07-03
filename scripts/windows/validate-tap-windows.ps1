#Requires -Version 5.1
[CmdletBinding()]
param(
    [string]$RepoRoot = "",
    [string]$PackageZip = "",
    [string]$ExtractTo = "",
    [string]$ExePath = "",
    [string]$ConfigPath = "",
    [string]$ReportPath = "",
    [switch]$RequireTap,
    [switch]$KeepExtracted
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$script:Lines = New-Object System.Collections.Generic.List[string]
$script:Failures = 0
$script:Warnings = 0
$script:CreatedExtractPath = ""

function Add-Line {
    param([string]$Text = "")
    $script:Lines.Add($Text) | Out-Null
    Write-Host $Text
}

function Add-Ok {
    param([Parameter(Mandatory = $true)][string]$Text)
    Add-Line "OK   $Text"
}

function Add-Warn {
    param([Parameter(Mandatory = $true)][string]$Text)
    $script:Warnings++
    Add-Line "WARN $Text"
}

function Add-Fail {
    param([Parameter(Mandatory = $true)][string]$Text)
    $script:Failures++
    Add-Line "FAIL $Text"
}

function Resolve-PackageRoot {
    if (-not [string]::IsNullOrWhiteSpace($PackageZip)) {
        $zipPath = (Resolve-Path -LiteralPath $PackageZip).Path
        if ([string]::IsNullOrWhiteSpace($ExtractTo)) {
            $ExtractTo = Join-Path ([System.IO.Path]::GetTempPath()) ("ntap-c-package-{0}" -f ([System.Guid]::NewGuid().ToString("N")))
            $script:CreatedExtractPath = $ExtractTo
        }
        New-Item -ItemType Directory -Path $ExtractTo -Force | Out-Null
        Expand-Archive -LiteralPath $zipPath -DestinationPath $ExtractTo -Force
        Add-Ok "extracted package: $zipPath"
        return (Resolve-Path -LiteralPath $ExtractTo).Path
    }

    $packagedRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
    if ((Test-Path -LiteralPath (Join-Path $packagedRoot "bin\ntap-c.exe")) -and
        (Test-Path -LiteralPath (Join-Path $packagedRoot "conf\ntap-c.conf.example"))) {
        return $packagedRoot
    }

    if ([string]::IsNullOrWhiteSpace($RepoRoot)) {
        $RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
    }
    return (Resolve-Path -LiteralPath $RepoRoot).Path
}

function Get-AdapterInventory {
    $items = @()
    if (Get-Command Get-NetAdapter -ErrorAction SilentlyContinue) {
        $items = @(Get-NetAdapter -ErrorAction SilentlyContinue | Select-Object Name, InterfaceDescription, Status, MacAddress, LinkSpeed)
    } else {
        $items = @(Get-CimInstance Win32_NetworkAdapter -ErrorAction SilentlyContinue |
            Where-Object { $_.NetConnectionID } |
            Select-Object @{Name="Name";Expression={$_.NetConnectionID}},
                          @{Name="InterfaceDescription";Expression={$_.Name}},
                          @{Name="Status";Expression={$_.NetConnectionStatus}},
                          MACAddress,
                          @{Name="LinkSpeed";Expression={$_.Speed}})
    }
    return @($items | Where-Object {
        ($_.Name -match "TAP|Wintun|WireGuard|OpenVPN") -or
        ($_.InterfaceDescription -match "TAP|Wintun|WireGuard|OpenVPN")
    })
}

function Quote-NativeArg {
    param([Parameter(Mandatory = $true)][string]$Value)
    return '"' + $Value.Replace('"', '\"') + '"'
}

function Invoke-NtapCheckEnv {
    param(
        [Parameter(Mandatory = $true)][string]$Exe,
        [Parameter(Mandatory = $true)][string]$Config
    )

    $binDir = Split-Path -Parent $Exe
    $pathParts = @($binDir)
    if (Test-Path -LiteralPath "C:\msys64\ucrt64\bin") {
        $pathParts += "C:\msys64\ucrt64\bin"
    }
    if (Test-Path -LiteralPath "C:\msys64\usr\bin") {
        $pathParts += "C:\msys64\usr\bin"
    }
    $pathParts += $env:PATH

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $Exe
    $psi.Arguments = "-c $(Quote-NativeArg -Value $Config) check-env"
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.EnvironmentVariables["PATH"] = ($pathParts -join ";")

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $psi
    [void]$process.Start()
    $stdout = $process.StandardOutput.ReadToEnd()
    $stderr = $process.StandardError.ReadToEnd()
    $process.WaitForExit()

    return [pscustomobject]@{
        ExitCode = $process.ExitCode
        Output = (($stdout, $stderr) | Where-Object {
            -not [string]::IsNullOrWhiteSpace($_)
        } | ForEach-Object {
            $_.TrimEnd()
        }) -join "`n"
    }
}

try {
    $packageRoot = Resolve-PackageRoot

    if ([string]::IsNullOrWhiteSpace($ReportPath)) {
        $ReportPath = Join-Path $packageRoot "ntap-c-tap-validation.txt"
    }

    if ([string]::IsNullOrWhiteSpace($ExePath)) {
        $packagedCli = Join-Path $packageRoot "bin\ntap-c-cli.exe"
        $packagedExe = Join-Path $packageRoot "bin\ntap-c.exe"
        if (Test-Path -LiteralPath $packagedCli) {
            $ExePath = $packagedCli
        } elseif (Test-Path -LiteralPath $packagedExe) {
            $ExePath = $packagedExe
        } else {
            $buildCli = Join-Path $packageRoot "build\msys2\bin\ntap-c-cli.exe"
            if (Test-Path -LiteralPath $buildCli) {
                $ExePath = $buildCli
            } else {
                $ExePath = Join-Path $packageRoot "build\msys2\bin\ntap-c.exe"
            }
        }
    }
    if ([string]::IsNullOrWhiteSpace($ConfigPath)) {
        $ConfigPath = Join-Path $packageRoot "conf\ntap-c.conf.example"
    }

    Add-Line "NTAP-C Windows TAP validation"
    Add-Line "PackageRoot=$packageRoot"
    Add-Line "ExePath=$ExePath"
    Add-Line "ConfigPath=$ConfigPath"
    Add-Line "RequireTap=$RequireTap"
    Add-Line ""

    if (Test-Path -LiteralPath $ExePath) {
        Add-Ok "ntap-c executable exists"
    } else {
        Add-Fail "missing ntap-c executable: $ExePath"
    }

    if (Test-Path -LiteralPath $ConfigPath) {
        Add-Ok "config example exists"
    } else {
        Add-Fail "missing config example: $ConfigPath"
    }

    $adapters = @(Get-AdapterInventory)
    if ($adapters.Count -eq 0) {
        Add-Warn "no TAP/Wintun/WireGuard/OpenVPN adapters detected"
    } else {
        Add-Ok ("adapter candidates detected: {0}" -f $adapters.Count)
        foreach ($adapter in $adapters) {
            Add-Line ("  {0} | {1} | {2}" -f $adapter.Name, $adapter.InterfaceDescription, $adapter.Status)
        }
    }

    $tapAdapters = @($adapters | Where-Object {
        ($_.Name -match "TAP|OpenVPN") -or ($_.InterfaceDescription -match "TAP|OpenVPN")
    })
    if ($tapAdapters.Count -gt 0) {
        Add-Ok "TAP-Windows6/OpenVPN style adapter candidate exists"
    } elseif ($RequireTap) {
        Add-Fail "TAP-Windows6/OpenVPN style adapter is required but was not found"
    } else {
        Add-Warn "TAP-Windows6/OpenVPN style adapter is not installed"
    }

    if ((Test-Path -LiteralPath $ExePath) -and (Test-Path -LiteralPath $ConfigPath)) {
        $result = Invoke-NtapCheckEnv -Exe $ExePath -Config $ConfigPath
        Add-Line ""
        Add-Line "ntap-c check-env output:"
        if ([string]::IsNullOrWhiteSpace($result.Output)) {
            Add-Line "  <no output>"
        } else {
            foreach ($line in ($result.Output -split "`r?`n")) {
                Add-Line "  | $line"
            }
        }

        if ($result.ExitCode -eq 0) {
            Add-Ok "ntap-c check-env exited 0"
        } else {
            $missingTap = $result.Output -match "tap_driver_check=missing|no TAP-Windows6 adapter found|requires TAP-Windows6"
            if ($missingTap -and -not $RequireTap) {
                Add-Warn "ntap-c check-env reported missing TAP adapter; non-strict validation continues"
            } else {
                Add-Fail ("ntap-c check-env exited {0}" -f $result.ExitCode)
            }
        }
    }

    Add-Line ""
    Add-Line ("Summary: failures={0} warnings={1}" -f $script:Failures, $script:Warnings)
    $reportDir = Split-Path -Parent $ReportPath
    if (-not [string]::IsNullOrWhiteSpace($reportDir)) {
        New-Item -ItemType Directory -Path $reportDir -Force | Out-Null
    }
    Add-Line "Report written: $ReportPath"
    Set-Content -LiteralPath $ReportPath -Value $script:Lines -Encoding ASCII

    if ($script:Failures -gt 0) {
        exit 1
    }
    exit 0
}
finally {
    if (-not $KeepExtracted -and -not [string]::IsNullOrWhiteSpace($script:CreatedExtractPath) -and
        (Test-Path -LiteralPath $script:CreatedExtractPath)) {
        $tempRoot = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath())
        $extractFull = [System.IO.Path]::GetFullPath($script:CreatedExtractPath)
        if ($extractFull.StartsWith($tempRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
            Remove-Item -LiteralPath $script:CreatedExtractPath -Recurse -Force
        }
    }
}
