# WinSCADA Power Agent — Uninstall Script
# Purpose: Cleanly removes the WinSCADA AI Optimized power scheme,
#          stops running agent/watchdog processes, and deletes telemetry logs.
# Usage:   Run from an elevated (Administrator) PowerShell prompt.

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$SchemeName = "WinSCADA AI Optimized"
$BalancedGuid = "381b4222-f694-41f0-9685-ff5bb260df2e"

# --- Step 1: Stop WinSCADA processes ---
Write-Host "[1/4] Stopping WinSCADA processes..." -ForegroundColor Cyan

foreach ($procName in @("agent", "watchdog")) {
    $procs = Get-Process -Name $procName -ErrorAction SilentlyContinue
    if ($procs) {
        $procs | Stop-Process -Force
        Write-Host "      Stopped '$procName' (PID: $($procs.Id -join ', '))" -ForegroundColor Yellow
    } else {
        Write-Host "      '$procName' is not running." -ForegroundColor DarkGray
    }
}

# --- Step 2: Identify the WinSCADA power scheme ---
Write-Host "[2/4] Enumerating power schemes..." -ForegroundColor Cyan

$activeSchemeOutput = powercfg /getactivescheme 2>&1
$activeGuid = $null
if ($activeSchemeOutput -match "([0-9a-fA-F\-]{36})") {
    $activeGuid = $Matches[1]
    Write-Host "      Active scheme GUID: $activeGuid" -ForegroundColor DarkGray
}

$winscadaGuid = $null
$schemeList = powercfg /list 2>&1
foreach ($line in $schemeList) {
    if ($line -match "([0-9a-fA-F\-]{36})" -and $line -like "*$SchemeName*") {
        $winscadaGuid = $Matches[1]
        break
    }
}

# --- Step 3: Switch away and delete the WinSCADA scheme ---
Write-Host "[3/4] Removing WinSCADA power scheme..." -ForegroundColor Cyan

if ($winscadaGuid) {
    # If the WinSCADA scheme is currently active, switch to Balanced first
    if ($activeGuid -and $activeGuid -eq $winscadaGuid) {
        Write-Host "      WinSCADA scheme is active — switching to Balanced first..." -ForegroundColor Yellow
        try {
            powercfg /setactive $BalancedGuid
            Write-Host "      Switched active scheme to Balanced ($BalancedGuid)." -ForegroundColor Green
        } catch {
            Write-Host "      ERROR: Failed to switch to Balanced scheme: $_" -ForegroundColor Red
            exit 1
        }
    }

    try {
        powercfg /delete $winscadaGuid
        Write-Host "      Deleted power scheme '$SchemeName' ($winscadaGuid)." -ForegroundColor Green
    } catch {
        Write-Host "      ERROR: Failed to delete power scheme: $_" -ForegroundColor Red
        exit 1
    }
} else {
    Write-Host "      '$SchemeName' power scheme not found — nothing to delete." -ForegroundColor DarkGray
}

# --- Step 4: Remove telemetry logs ---
Write-Host "[4/4] Cleaning up telemetry logs..." -ForegroundColor Cyan

$scriptDir = Split-Path -Parent $PSScriptRoot
$modelsDir = Join-Path $scriptDir "models"
$logPattern = Join-Path $modelsDir "telemetry_log*.csv"
$logs = Get-ChildItem -Path $logPattern -ErrorAction SilentlyContinue

if ($logs) {
    foreach ($log in $logs) {
        Remove-Item -Path $log.FullName -Force
        Write-Host "      Removed: $($log.Name)" -ForegroundColor Yellow
    }
} else {
    Write-Host "      No telemetry logs found in '$modelsDir'." -ForegroundColor DarkGray
}

Write-Host ""
Write-Host "WinSCADA Power Agent uninstall complete." -ForegroundColor Green
