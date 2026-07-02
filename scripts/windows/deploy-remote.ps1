#Requires -Version 5.1
[CmdletBinding()]
param(
    [string]$Version = "",
    [Parameter(Mandatory = $true)]
    [Alias("Host")]
    [string]$TargetHost,
    [int]$Port = 5985,
    [switch]$UseSSL,
    [System.Management.Automation.PSCredential]$Credential,
    [string]$CredentialUser = "",
    [string]$CredentialPassword = "",
    [string]$CredentialPasswordFile = "",
    [string]$CredentialPasswordEnv = "NTAP_WINDOWS_PASSWORD",
    [string]$RemoteDir = "",
    [string]$InstallRoot = "",
    [string]$ConfigPath = "",
    [string]$ServerAddr = "",
    [string]$Username = "",
    [string]$TapPassword = "",
    [string]$TapPasswordFile = "",
    [string]$TapPasswordEnv = "NTAP_TAP_PASSWORD",
    [string]$NetworkId = "1",
    [string]$TapName = "ntap-c0",
    [string]$Mtu = "1400",
    [switch]$RequireTap,
    [switch]$StartClient,
    [switch]$TargetDryRun,
    [switch]$DryRun,
    [string]$ReportOut = ""
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..\..")).Path
$PackageRoot = Join-Path $RepoRoot "_release\packages"
$ReportRoot = Join-Path $RepoRoot "_release\windows-tap\device-validation"

function Fail {
    param([Parameter(Mandatory = $true)][string]$Message)
    throw "Windows TAP remote deploy failed: $Message"
}

function Get-LatestPackageVersion {
    if (-not (Test-Path -LiteralPath $PackageRoot)) {
        Fail "package root not found: $PackageRoot"
    }
    $latest = Get-ChildItem -LiteralPath $PackageRoot -Directory | Sort-Object Name | Select-Object -Last 1
    if ($null -eq $latest) {
        Fail "no package version found under $PackageRoot"
    }
    return $latest.Name
}

function Quote-Ps {
    param([Parameter(Mandatory = $true)][string]$Value)
    return "'" + $Value.Replace("'", "''") + "'"
}

function Mask-Secrets {
    param([Parameter(Mandatory = $true)][string]$Text)
    if (-not [string]::IsNullOrWhiteSpace($script:ResolvedTapPassword)) {
        $Text = $Text.Replace($script:ResolvedTapPassword, "<masked>")
    }
    if (-not [string]::IsNullOrWhiteSpace($script:ResolvedCredentialPassword)) {
        $Text = $Text.Replace($script:ResolvedCredentialPassword, "<masked>")
    }
    return $Text
}

function Write-DryRun {
    param([Parameter(Mandatory = $true)][string]$Text)
    Write-Host ("DRY-RUN: {0}" -f (Mask-Secrets -Text $Text))
}

function Resolve-TapPassword {
    if (-not [string]::IsNullOrWhiteSpace($TapPassword)) {
        return $TapPassword
    }
    if (-not [string]::IsNullOrWhiteSpace($TapPasswordFile)) {
        if (-not (Test-Path -LiteralPath $TapPasswordFile)) {
            Fail "tap password file not found: $TapPasswordFile"
        }
        return (Get-Content -LiteralPath $TapPasswordFile -Raw).Trim()
    }
    if (-not [string]::IsNullOrWhiteSpace($TapPasswordEnv)) {
        $envValue = [Environment]::GetEnvironmentVariable($TapPasswordEnv)
        if (-not [string]::IsNullOrWhiteSpace($envValue)) {
            return $envValue
        }
    }
    return ""
}

function Resolve-SecretValue {
    param(
        [string]$Inline,
        [string]$FilePath,
        [string]$EnvName,
        [Parameter(Mandatory = $true)][string]$Label
    )
    if (-not [string]::IsNullOrWhiteSpace($Inline)) {
        return $Inline
    }
    if (-not [string]::IsNullOrWhiteSpace($FilePath)) {
        if (-not (Test-Path -LiteralPath $FilePath)) {
            Fail "$Label file not found: $FilePath"
        }
        return (Get-Content -LiteralPath $FilePath -Raw).Trim()
    }
    if (-not [string]::IsNullOrWhiteSpace($EnvName)) {
        $envValue = [Environment]::GetEnvironmentVariable($EnvName)
        if (-not [string]::IsNullOrWhiteSpace($envValue)) {
            return $envValue
        }
    }
    return ""
}

function New-WindowsCredential {
    if ($null -ne $Credential) {
        return $Credential
    }
    if ([string]::IsNullOrWhiteSpace($CredentialUser)) {
        if (-not [string]::IsNullOrWhiteSpace($CredentialPassword) -or
            -not [string]::IsNullOrWhiteSpace($CredentialPasswordFile)) {
            Fail "CredentialUser is required when a Windows credential password source is configured"
        }
        return $null
    }
    $script:ResolvedCredentialPassword = Resolve-SecretValue -Inline $CredentialPassword -FilePath $CredentialPasswordFile -EnvName $CredentialPasswordEnv -Label "Windows credential password"
    if ([string]::IsNullOrWhiteSpace($script:ResolvedCredentialPassword)) {
        Fail "Windows credential password is required when CredentialUser is set"
    }
    $secure = ConvertTo-SecureString -String $script:ResolvedCredentialPassword -AsPlainText -Force
    return New-Object System.Management.Automation.PSCredential($CredentialUser, $secure)
}

function New-RemoteSession {
    $sessionArgs = @{
        ComputerName = $TargetHost
        Port = $Port
    }
    if ($UseSSL) {
        $sessionArgs.UseSSL = $true
    }
    if ($null -ne $script:ResolvedCredential) {
        $sessionArgs.Credential = $script:ResolvedCredential
    }
    return New-PSSession @sessionArgs
}

if ([string]::IsNullOrWhiteSpace($Version)) {
    $Version = Get-LatestPackageVersion
}

$packageDir = Join-Path $PackageRoot $Version
if (-not (Test-Path -LiteralPath $packageDir)) {
    Fail "package version not found: $packageDir"
}

$packageZip = Join-Path $packageDir "NTAP-C-$Version-windows-x64.zip"
if (-not (Test-Path -LiteralPath $packageZip)) {
    Fail "missing NTAP-C Windows package: $packageZip"
}

$script:ResolvedTapPassword = Resolve-TapPassword
$script:ResolvedCredentialPassword = ""
$script:ResolvedCredential = New-WindowsCredential
foreach ($pair in @(
    @{ Name = "ServerAddr"; Value = $ServerAddr },
    @{ Name = "Username"; Value = $Username },
    @{ Name = "TapPassword/TapPasswordFile/$TapPasswordEnv"; Value = $script:ResolvedTapPassword }
)) {
    if ([string]::IsNullOrWhiteSpace($pair.Value)) {
        Fail "$($pair.Name) is required"
    }
}

if ([string]::IsNullOrWhiteSpace($RemoteDir)) {
    $RemoteDir = "C:\Windows\Temp\ntap-$Version"
}
if ([string]::IsNullOrWhiteSpace($InstallRoot)) {
    $InstallRoot = Join-Path $RemoteDir "install"
}
if ([string]::IsNullOrWhiteSpace($ConfigPath)) {
    $ConfigPath = Join-Path $RemoteDir "ntap-c.conf"
}

$safeHost = ($TargetHost -replace '[^A-Za-z0-9_.-]', '_')
if ([string]::IsNullOrWhiteSpace($ReportOut)) {
    $ReportOut = Join-Path $ReportRoot "$Version-$safeHost.txt"
}

$remoteZip = Join-Path $RemoteDir (Split-Path -Leaf $packageZip)
$remotePackageRoot = Join-Path $RemoteDir "package"
$remoteInstaller = Join-Path $remotePackageRoot "install\install-ntap-c.ps1"
$remoteReport = Join-Path $InstallRoot "ntap-c-tap-validation.txt"

Write-Host "NTAP-C Windows TAP remote deploy"
Write-Host "Version=$Version"
Write-Host "TargetHost=$TargetHost"
Write-Host "Port=$Port"
Write-Host "UseSSL=$UseSSL"
if ($null -ne $script:ResolvedCredential) {
    Write-Host "CredentialUser=$($script:ResolvedCredential.UserName)"
}
Write-Host "RemoteDir=$RemoteDir"
Write-Host "Package=$packageZip"
Write-Host "InstallRoot=$InstallRoot"
Write-Host "ConfigPath=$ConfigPath"
Write-Host "ReportOut=$ReportOut"
Write-Host "RequireTap=$RequireTap"
Write-Host "TargetDryRun=$TargetDryRun"
Write-Host "DryRun=$DryRun"

$installArgDisplay = @(
    "powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", (Quote-Ps $remoteInstaller),
    "-PackageRoot", (Quote-Ps $remotePackageRoot),
    "-InstallRoot", (Quote-Ps $InstallRoot),
    "-ConfigPath", (Quote-Ps $ConfigPath),
    "-ServerAddr", (Quote-Ps $ServerAddr),
    "-Username", (Quote-Ps $Username),
    "-Password", "<masked>",
    "-NetworkId", (Quote-Ps $NetworkId),
    "-TapName", (Quote-Ps $TapName),
    "-Mtu", (Quote-Ps $Mtu),
    "-ForceConfig",
    "-RunValidation"
)
if ($RequireTap) { $installArgDisplay += "-RequireTap" }
if ($StartClient) { $installArgDisplay += "-StartClient" }
if ($TargetDryRun) { $installArgDisplay += "-DryRun" }

if ($DryRun) {
    if ($null -ne $script:ResolvedCredential) {
        Write-DryRun "New-PSSession -ComputerName $(Quote-Ps $TargetHost) -Port $Port -Credential <credential:$($script:ResolvedCredential.UserName)>"
    } else {
        Write-DryRun "New-PSSession -ComputerName $(Quote-Ps $TargetHost) -Port $Port"
    }
    Write-DryRun "Invoke-Command: New-Item -ItemType Directory -Path $(Quote-Ps $RemoteDir) -Force"
    Write-DryRun "Copy-Item -ToSession <session> -LiteralPath $(Quote-Ps $packageZip) -Destination $(Quote-Ps $remoteZip) -Force"
    Write-DryRun "Invoke-Command: Expand-Archive -LiteralPath $(Quote-Ps $remoteZip) -DestinationPath $(Quote-Ps $remotePackageRoot) -Force"
    Write-DryRun ("Invoke-Command: {0}" -f ($installArgDisplay -join " "))
    if ($TargetDryRun) {
        Write-DryRun "Remote report copy skipped for target dry-run."
    } else {
        Write-DryRun "Copy-Item -FromSession <session> -Path $(Quote-Ps $remoteReport) -Destination $(Quote-Ps $ReportOut) -Force"
    }
    exit 0
}

$session = $null
try {
    $session = New-RemoteSession
    Invoke-Command -Session $session -ScriptBlock {
        param($Path)
        New-Item -ItemType Directory -Path $Path -Force | Out-Null
    } -ArgumentList $RemoteDir

    Copy-Item -ToSession $session -LiteralPath $packageZip -Destination $remoteZip -Force

    Invoke-Command -Session $session -ScriptBlock {
        param(
            $ZipPath,
            $PackageRoot,
            $Installer,
            $InstallRoot,
            $ConfigPath,
            $ServerAddr,
            $Username,
            $Password,
            $NetworkId,
            $TapName,
            $Mtu,
            $RequireTap,
            $StartClient,
            $TargetDryRun
        )
        if (Test-Path -LiteralPath $PackageRoot) {
            Remove-Item -LiteralPath $PackageRoot -Recurse -Force
        }
        New-Item -ItemType Directory -Path $PackageRoot -Force | Out-Null
        Expand-Archive -LiteralPath $ZipPath -DestinationPath $PackageRoot -Force
        if (-not (Test-Path -LiteralPath $Installer)) {
            throw "missing packaged installer: $Installer"
        }
        $args = @(
            "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $Installer,
            "-PackageRoot", $PackageRoot,
            "-InstallRoot", $InstallRoot,
            "-ConfigPath", $ConfigPath,
            "-ServerAddr", $ServerAddr,
            "-Username", $Username,
            "-Password", $Password,
            "-NetworkId", $NetworkId,
            "-TapName", $TapName,
            "-Mtu", $Mtu,
            "-ForceConfig",
            "-RunValidation"
        )
        if ($RequireTap) { $args += "-RequireTap" }
        if ($StartClient) { $args += "-StartClient" }
        if ($TargetDryRun) { $args += "-DryRun" }
        & powershell @args
        if ($LASTEXITCODE -ne 0) {
            throw "packaged NTAP-C install/validation failed with exit code $LASTEXITCODE"
        }
    } -ArgumentList $remoteZip, $remotePackageRoot, $remoteInstaller, $InstallRoot, $ConfigPath, $ServerAddr, $Username, $script:ResolvedTapPassword, $NetworkId, $TapName, $Mtu, [bool]$RequireTap, [bool]$StartClient, [bool]$TargetDryRun

    if ($TargetDryRun) {
        Write-Host "Remote report copy skipped for target dry-run."
    } else {
        $reportDir = Split-Path -Parent $ReportOut
        if (-not [string]::IsNullOrWhiteSpace($reportDir)) {
            New-Item -ItemType Directory -Path $reportDir -Force | Out-Null
        }
        Copy-Item -FromSession $session -Path $remoteReport -Destination $ReportOut -Force
        Write-Host "Windows TAP validation report: $ReportOut"
    }
}
finally {
    if ($null -ne $session) {
        Remove-PSSession -Session $session
    }
}
