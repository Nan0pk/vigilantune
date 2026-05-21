# nanoloop Power Agent — Uninstall Script
# Purpose: Cleanly removes both old WinSCADA and new Nanoloop AI Optimized power schemes,
#          stops running agent/watchdog processes, and deletes telemetry logs.
# Usage:   Run from an elevated (Administrator) PowerShell prompt.

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$SchemeNames = @("WinSCADA AI Optimized", "Nanoloop AI Optimized")
$BalancedGuid = "381b4222-f694-41f0-9685-ff5bb260df2e"

# --- Step 1: Stop agent/watchdog processes ---
Write-Host "[1/4] Stopping agent and watchdog processes..." -ForegroundColor Cyan

foreach ($procName in @("agent", "watchdog")) {
    $procs = Get-Process -Name $procName -ErrorAction SilentlyContinue
    if ($procs) {
        $procs | Stop-Process -Force
        Write-Host "      Stopped '$procName' (PID: $($procs.Id -join ', '))" -ForegroundColor Yellow
    } else {
        Write-Host "      '$procName' is not running." -ForegroundColor DarkGray
    }
}

# --- Step 2: Identify the power schemes to delete ---
Write-Host "[2/4] Enumerating power schemes..." -ForegroundColor Cyan

$activeSchemeOutput = powercfg /getactivescheme 2>&1
$activeGuid = $null
if ($activeSchemeOutput -match "([0-9a-fA-F\-]{36})") {
    $activeGuid = $Matches[1]
    Write-Host "      Active scheme GUID: $activeGuid" -ForegroundColor DarkGray
}

$guidsToDelete = @()
$schemeList = powercfg /list 2>&1
foreach ($line in $schemeList) {
    if ($line -match "([0-9a-fA-F\-]{36})") {
        $guid = $Matches[1]
        foreach ($name in $SchemeNames) {
            if ($line -like "*$name*") {
                $guidsToDelete += [PSCustomObject]@{
                    Guid = $guid
                    Name = $name
                }
            }
        }
    }
}

# --- Step 3: Switch away and delete the power schemes ---
Write-Host "[3/4] Removing power schemes..." -ForegroundColor Cyan

if ($guidsToDelete.Count -gt 0) {
    foreach ($scheme in $guidsToDelete) {
        # If the scheme to delete is currently active, switch to Balanced first
        if ($activeGuid -and $activeGuid -eq $scheme.Guid) {
            Write-Host "      Active scheme '$($scheme.Name)' is active — switching to Balanced first..." -ForegroundColor Yellow
            try {
                powercfg /setactive $BalancedGuid
                $activeGuid = $BalancedGuid
                Write-Host "      Switched active scheme to Balanced ($BalancedGuid)." -ForegroundColor Green
            } catch {
                Write-Host "      ERROR: Failed to switch to Balanced scheme: $_" -ForegroundColor Red
                exit 1
            }
        }

        try {
            powercfg /delete $($scheme.Guid)
            Write-Host "      Deleted power scheme '$($scheme.Name)' ($($scheme.Guid))." -ForegroundColor Green
        } catch {
            Write-Host "      ERROR: Failed to delete power scheme: $_" -ForegroundColor Red
            exit 1
        }
    }
} else {
    Write-Host "      No nanoloop/WinSCADA power schemes found — nothing to delete." -ForegroundColor DarkGray
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
Write-Host "nanoloop Power Agent uninstall complete." -ForegroundColor Green
