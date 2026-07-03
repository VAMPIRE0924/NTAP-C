#Requires -Version 5.1
[CmdletBinding()]
param(
    [string]$RepoRoot = "",
    [string]$ExePath = "",
    [string]$ConfigPath = "",
    [switch]$Build,
    [switch]$RequireTap
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

function ConvertTo-MsysPath {
    param([Parameter(Mandatory = $true)][string]$Path)

    $full = [System.IO.Path]::GetFullPath($Path)
    $drive = $full.Substring(0, 1).ToLowerInvariant()
    $rest = $full.Substring(2).Replace('\', '/')
    return "/$drive$rest"
}

function Quote-NativeArg {
    param([Parameter(Mandatory = $true)][string]$Value)

    return '"' + $Value.Replace('"', '\"') + '"'
}

if ([string]::IsNullOrWhiteSpace($RepoRoot)) {
    $RepoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..\..")).Path
} else {
    $RepoRoot = (Resolve-Path -LiteralPath $RepoRoot).Path
}

if ([string]::IsNullOrWhiteSpace($ExePath)) {
    $ExePath = Join-Path $RepoRoot "build\msys2\bin\ntap-c-cli.exe"
}
if ([string]::IsNullOrWhiteSpace($ConfigPath)) {
    $ConfigPath = Join-Path $RepoRoot "conf\ntap-c.conf.example"
}

if ($Build) {
    $buildScript = Join-Path $RepoRoot "scripts\build-msys2.ps1"
    if (Test-Path -LiteralPath $buildScript) {
        powershell -NoProfile -ExecutionPolicy Bypass -File $buildScript
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    } else {
        $bash = "C:\msys64\usr\bin\bash.exe"
        if (-not (Test-Path -LiteralPath $bash)) {
            throw "MSYS2 bash not found: $bash"
        }
        $msysRepo = ConvertTo-MsysPath -Path $RepoRoot
        & $bash -lc "export PATH=/ucrt64/bin:/usr/bin:/bin:`$PATH; cd '$msysRepo' && make"
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }
}

if (-not (Test-Path -LiteralPath $ExePath)) {
    throw "Missing ntap-c CLI executable: $ExePath"
}
if (-not (Test-Path -LiteralPath $ConfigPath)) {
    throw "Missing ntap-c config: $ConfigPath"
}

$env:PATH = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin;$env:PATH"
$processInfo = New-Object System.Diagnostics.ProcessStartInfo
$processInfo.FileName = $ExePath
$processInfo.Arguments = "-c $(Quote-NativeArg -Value $ConfigPath) check-env"
$processInfo.UseShellExecute = $false
$processInfo.RedirectStandardOutput = $true
$processInfo.RedirectStandardError = $true
$processInfo.EnvironmentVariables["PATH"] = $env:PATH
$process = New-Object System.Diagnostics.Process
$process.StartInfo = $processInfo
[void]$process.Start()
$stdout = $process.StandardOutput.ReadToEnd()
$stderr = $process.StandardError.ReadToEnd()
$process.WaitForExit()
$exitCode = $process.ExitCode
$text = (($stdout, $stderr) | Where-Object {
    -not [string]::IsNullOrWhiteSpace($_)
} | ForEach-Object {
    $_.TrimEnd()
}) -join "`n"
if (-not [string]::IsNullOrWhiteSpace($text)) {
    Write-Output $text
}

if ($exitCode -eq 0) {
    Write-Host "windows TAP-Windows6 smoke: ok"
    exit 0
}

$missingTap = $text -match "tap_driver_check=missing" -or
              $text -match "no TAP-Windows6 adapter found" -or
              $text -match "requires TAP-Windows6"
if ($missingTap -and -not $RequireTap) {
    Write-Host "windows TAP-Windows6 smoke: skipped; install TAP-Windows6 and rerun with -RequireTap for strict validation"
    exit 0
}

[Console]::Error.WriteLine("windows TAP-Windows6 smoke failed")
exit $exitCode
