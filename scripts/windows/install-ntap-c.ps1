#Requires -Version 5.1
[CmdletBinding()]
param(
    [string]$PackageZip = "",
    [string]$ExtractTo = "",
    [string]$PackageRoot = "",
    [string]$InstallRoot = "",
    [string]$ConfigPath = "",
    [string]$ServerAddr = "",
    [string]$Username = "",
    [string]$Password = "",
    [string]$NetworkId = "1",
    [string]$TapName = "ntap-c0",
    [string]$Mtu = "1400",
    [string]$LogLevel = "info",
    [string]$LogFile = "",
    [switch]$ForceConfig,
    [switch]$RunValidation,
    [switch]$RequireTap,
    [switch]$StartClient,
    [switch]$DryRun,
    [switch]$KeepExtracted
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"
$script:CreatedExtractPath = ""

function Write-Step {
    param([Parameter(Mandatory = $true)][string]$Message)
    Write-Host $Message
}

function Invoke-InstallAction {
    param(
        [Parameter(Mandatory = $true)][string]$Label,
        [Parameter(Mandatory = $true)][scriptblock]$Action
    )

    if ($DryRun) {
        Write-Step "DRY-RUN: $Label"
    } else {
        & $Action
    }
}

function Get-DefaultInstallRoot {
    $programFiles = [Environment]::GetFolderPath("ProgramFiles")
    if ([string]::IsNullOrWhiteSpace($programFiles)) {
        $programFiles = Join-Path $env:SystemDrive "Program Files"
    }
    return Join-Path $programFiles "NTAP\ntap-c"
}

function Get-DefaultConfigPath {
    $programData = [Environment]::GetFolderPath("CommonApplicationData")
    if ([string]::IsNullOrWhiteSpace($programData)) {
        $programData = Join-Path $env:SystemDrive "ProgramData"
    }
    return Join-Path $programData "NTAP\ntap-c.conf"
}

function Resolve-InputPackageRoot {
    if (-not [string]::IsNullOrWhiteSpace($PackageZip)) {
        $zipPath = (Resolve-Path -LiteralPath $PackageZip).Path
        if ([string]::IsNullOrWhiteSpace($ExtractTo)) {
            $ExtractTo = Join-Path ([System.IO.Path]::GetTempPath()) ("ntap-c-install-{0}" -f ([System.Guid]::NewGuid().ToString("N")))
            $script:CreatedExtractPath = $ExtractTo
        }
        Invoke-InstallAction "extract $zipPath -> $ExtractTo" {
            New-Item -ItemType Directory -Path $ExtractTo -Force | Out-Null
            Expand-Archive -LiteralPath $zipPath -DestinationPath $ExtractTo -Force
        }
        return [System.IO.Path]::GetFullPath($ExtractTo)
    }

    if (-not [string]::IsNullOrWhiteSpace($PackageRoot)) {
        return (Resolve-Path -LiteralPath $PackageRoot).Path
    }

    $packagedRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
    if ((Test-Path -LiteralPath (Join-Path $packagedRoot "bin\ntap-c.exe")) -and
        (Test-Path -LiteralPath (Join-Path $packagedRoot "conf\ntap-c.conf.example"))) {
        return $packagedRoot
    }

    return [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
}

function Write-ClientConfig {
    param([Parameter(Mandatory = $true)][string]$Path)

    $missing = @()
    if ([string]::IsNullOrWhiteSpace($ServerAddr)) { $missing += "ServerAddr" }
    if ([string]::IsNullOrWhiteSpace($Username)) { $missing += "Username" }
    if ([string]::IsNullOrWhiteSpace($Password)) { $missing += "Password" }
    if ($missing.Count -gt 0) {
        throw "Config generation requires: $($missing -join ', '). Pass -ForceConfig only with full client credentials."
    }

    $content = @"
[client]
server_addr=$ServerAddr
username=$Username
password=$Password
network_id=$NetworkId
tap_name=$TapName
mtu=$Mtu

[log]
level=$LogLevel
file=$LogFile
"@
    $configDir = Split-Path -Parent $Path
    Invoke-InstallAction "create config directory $configDir" {
        New-Item -ItemType Directory -Path $configDir -Force | Out-Null
    }
    Invoke-InstallAction "write config $Path with password=<masked>" {
        Set-Content -LiteralPath $Path -Value $content -Encoding ASCII
    }
}

try {
    $sourceRoot = Resolve-InputPackageRoot
    if ([string]::IsNullOrWhiteSpace($InstallRoot)) {
        $InstallRoot = Get-DefaultInstallRoot
    }
    if ([string]::IsNullOrWhiteSpace($ConfigPath)) {
        $ConfigPath = Get-DefaultConfigPath
    }
    if ([string]::IsNullOrWhiteSpace($LogFile)) {
        $logDir = Split-Path -Parent $ConfigPath
        $LogFile = Join-Path $logDir "ntap-c.log"
    }

    $sourceExe = Join-Path $sourceRoot "bin\ntap-c.exe"
    $sourceCliExe = Join-Path $sourceRoot "bin\ntap-c-cli.exe"
    $sourceConf = Join-Path $sourceRoot "conf\ntap-c.conf.example"
    if (-not $DryRun) {
        if (-not (Test-Path -LiteralPath $sourceExe)) {
            throw "missing ntap-c executable: $sourceExe"
        }
        if (-not (Test-Path -LiteralPath $sourceCliExe)) {
            throw "missing ntap-c CLI executable: $sourceCliExe"
        }
        if (-not (Test-Path -LiteralPath $sourceConf)) {
            throw "missing config example: $sourceConf"
        }
    }

    Write-Step "NTAP-C Windows install"
    Write-Step "PackageRoot=$sourceRoot"
    Write-Step "InstallRoot=$InstallRoot"
    Write-Step "ConfigPath=$ConfigPath"
    Write-Step "ServerAddr=$ServerAddr"
    Write-Step "Username=$Username"
    Write-Step "Password=<masked>"

    Invoke-InstallAction "create install root $InstallRoot" {
        New-Item -ItemType Directory -Path $InstallRoot -Force | Out-Null
    }

    foreach ($dir in @("bin", "conf", "validate", "install")) {
        $sourceDir = Join-Path $sourceRoot $dir
        if (Test-Path -LiteralPath $sourceDir) {
            $targetDir = Join-Path $InstallRoot $dir
            Invoke-InstallAction "copy $dir -> $targetDir" {
                if (Test-Path -LiteralPath $targetDir) {
                    Remove-Item -LiteralPath $targetDir -Recurse -Force
                }
                Copy-Item -LiteralPath $sourceDir -Destination $targetDir -Recurse -Force
            }
        }
    }

    if ((Test-Path -LiteralPath $ConfigPath) -and -not $ForceConfig) {
        Write-Step "Config exists, keeping current file: $ConfigPath"
    } elseif ([string]::IsNullOrWhiteSpace($ServerAddr) -and [string]::IsNullOrWhiteSpace($Username) -and [string]::IsNullOrWhiteSpace($Password)) {
        $configDir = Split-Path -Parent $ConfigPath
        Invoke-InstallAction "copy example config to $ConfigPath" {
            New-Item -ItemType Directory -Path $configDir -Force | Out-Null
            Copy-Item -LiteralPath $sourceConf -Destination $ConfigPath -Force
        }
        Write-Step "Config template installed; edit before production use: $ConfigPath"
    } else {
        Write-ClientConfig -Path $ConfigPath
    }

    $installedExe = Join-Path $InstallRoot "bin\ntap-c.exe"
    $installedCliExe = Join-Path $InstallRoot "bin\ntap-c-cli.exe"
    $validator = Join-Path $InstallRoot "validate\validate-tap-windows.ps1"

    if ($RunValidation) {
        if (-not $DryRun -and -not (Test-Path -LiteralPath $validator)) {
            throw "missing validator: $validator"
        }
        $validateArgs = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $validator, "-ExePath", $installedCliExe, "-ConfigPath", $ConfigPath)
        if ($RequireTap) {
            $validateArgs += "-RequireTap"
        }
        if ($DryRun) {
            Write-Step ("DRY-RUN: powershell {0}" -f ($validateArgs -join " "))
        } else {
            & powershell @validateArgs
            if ($LASTEXITCODE -ne 0) {
                exit $LASTEXITCODE
            }
        }
    }

    if ($StartClient) {
        $arguments = "-c `"$ConfigPath`" run"
        Invoke-InstallAction "start ntap-c CLI client in hidden window" {
            Start-Process -FilePath $installedCliExe -ArgumentList $arguments -WorkingDirectory $InstallRoot -WindowStyle Hidden
        }
    }

    Write-Step "Install complete."
    Write-Step "Customer GUI: $installedExe"
    Write-Step "Edit config before production use: $ConfigPath"
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
