#Requires -Version 5.1
[CmdletBinding()]
param(
    [string]$TapName = "ntap-c0",
    [switch]$RequireCreated
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

function Write-Step {
    param([Parameter(Mandatory = $true)][string]$Message)
    Write-Host $Message
}

function Test-IsAdmin {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Get-TapAdapters {
    if (Get-Command Get-NetAdapter -ErrorAction SilentlyContinue) {
        return @(Get-NetAdapter -ErrorAction SilentlyContinue | Where-Object {
            $_.InterfaceDescription -match 'TAP|OpenVPN' -or $_.Name -match 'TAP|OpenVPN|ntap'
        })
    }
    return @(Get-CimInstance Win32_NetworkAdapter -ErrorAction SilentlyContinue | Where-Object {
        $_.NetConnectionID -and ($_.Name -match 'TAP|OpenVPN' -or $_.NetConnectionID -match 'TAP|OpenVPN|ntap')
    })
}

function Rename-SingleTap {
    param([Parameter(Mandatory = $true)]$Adapter)
    if ([string]::IsNullOrWhiteSpace($TapName) -or $Adapter.Name -eq $TapName) {
        return
    }
    if (Get-Command Rename-NetAdapter -ErrorAction SilentlyContinue) {
        Write-Step "Renaming TAP adapter '$($Adapter.Name)' -> '$TapName'"
        Rename-NetAdapter -Name $Adapter.Name -NewName $TapName -ErrorAction Stop
    }
}

function Find-FirstExistingPath {
    param([Parameter(Mandatory = $true)][string[]]$Paths)
    foreach ($path in $Paths) {
        $expanded = [Environment]::ExpandEnvironmentVariables($path)
        if (Test-Path -LiteralPath $expanded -PathType Leaf) {
            return (Resolve-Path -LiteralPath $expanded).Path
        }
    }
    return ""
}

function Invoke-Native {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )
    Write-Step ("Running: {0} {1}" -f $FilePath, ($Arguments -join " "))
    $process = Start-Process -FilePath $FilePath -ArgumentList $Arguments -Wait -PassThru -WindowStyle Hidden
    Write-Step ("ExitCode={0}" -f $process.ExitCode)
    return $process.ExitCode
}

function New-TapWithTapctl {
    $tapctl = Find-FirstExistingPath @(
        "$PSScriptRoot\tapctl.exe",
        "$PSScriptRoot\..\bin\tapctl.exe",
        "%ProgramFiles%\OpenVPN\bin\tapctl.exe",
        "%ProgramFiles(x86)%\OpenVPN\bin\tapctl.exe",
        "%ProgramFiles%\TAP-Windows\bin\tapctl.exe",
        "%ProgramFiles(x86)%\TAP-Windows\bin\tapctl.exe"
    )
    if ([string]::IsNullOrWhiteSpace($tapctl)) {
        return $false
    }
    $args = @("create")
    if (-not [string]::IsNullOrWhiteSpace($TapName)) {
        $args += @("--name", $TapName)
    }
    return (Invoke-Native -FilePath $tapctl -Arguments $args) -eq 0
}

function Find-TapInf {
    return Find-FirstExistingPath @(
        "$PSScriptRoot\drivers\tap-windows6\OemVista.inf",
        "$PSScriptRoot\..\drivers\tap-windows6\OemVista.inf",
        "%ProgramFiles%\TAP-Windows\driver\OemVista.inf",
        "%ProgramFiles(x86)%\TAP-Windows\driver\OemVista.inf",
        "%ProgramFiles%\OpenVPN\driver\OemVista.inf",
        "%ProgramFiles(x86)%\OpenVPN\driver\OemVista.inf"
    )
}

function New-TapWithTapinstall {
    $tapinstall = Find-FirstExistingPath @(
        "$PSScriptRoot\tapinstall.exe",
        "$PSScriptRoot\..\bin\tapinstall.exe",
        "%ProgramFiles%\TAP-Windows\bin\tapinstall.exe",
        "%ProgramFiles(x86)%\TAP-Windows\bin\tapinstall.exe",
        "%ProgramFiles%\OpenVPN\bin\tapinstall.exe",
        "%ProgramFiles(x86)%\OpenVPN\bin\tapinstall.exe"
    )
    $inf = Find-TapInf
    if ([string]::IsNullOrWhiteSpace($tapinstall) -or [string]::IsNullOrWhiteSpace($inf)) {
        return $false
    }
    return (Invoke-Native -FilePath $tapinstall -Arguments @("install", $inf, "tap0901")) -eq 0
}

function New-TapWithDevcon {
    $devcon = Find-FirstExistingPath @(
        "$PSScriptRoot\devcon.exe",
        "$PSScriptRoot\..\bin\devcon.exe",
        "%ProgramFiles%\TAP-Windows\bin\devcon.exe",
        "%ProgramFiles(x86)%\TAP-Windows\bin\devcon.exe",
        "%ProgramFiles%\OpenVPN\bin\devcon.exe",
        "%ProgramFiles(x86)%\OpenVPN\bin\devcon.exe"
    )
    $inf = Find-TapInf
    if ([string]::IsNullOrWhiteSpace($devcon) -or [string]::IsNullOrWhiteSpace($inf)) {
        return $false
    }
    return (Invoke-Native -FilePath $devcon -Arguments @("install", $inf, "tap0901")) -eq 0
}

function Install-TapDriverWithPnPUtil {
    $inf = Find-TapInf
    if ([string]::IsNullOrWhiteSpace($inf)) {
        return $false
    }
    $pnputil = Join-Path $env:SystemRoot "System32\pnputil.exe"
    if (-not (Test-Path -LiteralPath $pnputil -PathType Leaf)) {
        return $false
    }
    return (Invoke-Native -FilePath $pnputil -Arguments @("/add-driver", $inf, "/install")) -eq 0
}

Write-Step "NTAP-C TAP-Windows preparation"
Write-Step "TapName=$TapName"
Write-Step ("IsAdmin={0}" -f (Test-IsAdmin))

if (-not (Test-IsAdmin)) {
    throw "administrator privileges are required to create or rename TAP adapters"
}

$before = @(Get-TapAdapters)
if ($before.Count -gt 0) {
    Write-Step ("Existing TAP/OpenVPN adapter count={0}" -f $before.Count)
    if ($before.Count -eq 1) {
        Rename-SingleTap -Adapter $before[0]
    }
    Write-Step "TAP adapter already available."
    exit 0
}

$created = $false
if (-not $created) { $created = New-TapWithTapctl }
if (-not $created) { $created = New-TapWithTapinstall }
if (-not $created) { $created = New-TapWithDevcon }
if (-not $created) { [void](Install-TapDriverWithPnPUtil) }

Start-Sleep -Seconds 2
$after = @(Get-TapAdapters)
if ($after.Count -gt 0) {
    Write-Step ("TAP/OpenVPN adapter count after preparation={0}" -f $after.Count)
    if ($after.Count -eq 1) {
        Rename-SingleTap -Adapter $after[0]
    }
    Write-Step "TAP adapter ready."
    exit 0
}

if ($RequireCreated) {
    throw "TAP adapter was not created"
}

throw "No TAP-Windows6/OpenVPN TAP adapter is available. Install OpenVPN TAP-Windows6 driver or provide tapctl/tapinstall with OemVista.inf, then retry."
