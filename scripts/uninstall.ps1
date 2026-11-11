# VigilanTune Power Agent — safe pre-alpha cleanup
# Stops only verified executables inside this installation. Power plans and
# telemetry remain untouched until ownership and restoration are implemented.

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$separators = [char[]]@([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)
$installRoot = ([System.IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))).TrimEnd($separators)
$installPrefix = $installRoot + [System.IO.Path]::DirectorySeparatorChar

function Get-InstalledExecutableStatus {
    param([string]$Path, [string]$ExpectedName)

    try {
        $fullPath = [System.IO.Path]::GetFullPath($Path)
        if (-not $fullPath.StartsWith($installPrefix, [System.StringComparison]::Ordinal)) {
            if ($fullPath.StartsWith($installPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
                return "unverified"
            }
            return "unrelated"
        }
        $fileName = [System.IO.Path]::GetFileName($fullPath)
        if (-not [string]::Equals($fileName, $ExpectedName, [System.StringComparison]::Ordinal)) {
            if ([string]::Equals($fileName, $ExpectedName, [System.StringComparison]::OrdinalIgnoreCase)) {
                return "unverified"
            }
            return "unrelated"
        }

        $entry = Get-Item -LiteralPath $fullPath -Force -ErrorAction Stop
        if ($entry -isnot [System.IO.FileInfo]) {
            return "unverified"
        }
        while ($null -ne $entry) {
            if (($entry.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
                return "unverified"
            }
            $entryPath = ([System.IO.Path]::GetFullPath($entry.FullName)).TrimEnd($separators)
            if ([string]::Equals($entryPath, $installRoot, [System.StringComparison]::Ordinal)) {
                return "verified"
            }
            if ($entry -is [System.IO.FileInfo]) {
                $entry = $entry.Directory
            } else {
                $entry = $entry.Parent
            }
        }
    } catch {
        return "unverified"
    }
    return "unverified"
}

Write-Host "Stopping processes belonging to this VigilanTune installation..." -ForegroundColor Cyan
$cleanupIncomplete = $false
$watchdogUnresolved = $false
foreach ($name in @("VigilanTune.Watchdog", "VigilanTune.Agent")) {
    if ($name -eq "VigilanTune.Agent" -and $watchdogUnresolved) {
        Write-Warning "Skipped agent cleanup because a watchdog may still be running."
        break
    }
    try {
        $snapshot = @(Get-Process -ErrorAction Stop)
        if ($name -eq "VigilanTune.Agent" -and
            @($snapshot | Where-Object { $_.ProcessName -ieq "VigilanTune.Watchdog" }).Count -gt 0) {
            $cleanupIncomplete = $true
            Write-Warning "Skipped agent cleanup because a watchdog is still running."
            break
        }
        $candidates = @($snapshot | Where-Object { $_.ProcessName -ieq $name })
    } catch {
        $cleanupIncomplete = $true
        if ($name -eq "VigilanTune.Watchdog") { $watchdogUnresolved = $true }
        Write-Warning "Could not enumerate processes: $_"
        continue
    }
    foreach ($process in $candidates) {
        try {
            # Keep the process object tied to this OS process while checking
            # its path and stopping it, even if the process exits meanwhile.
            $processHandle = $process.SafeHandle
            if ($null -eq $processHandle -or $processHandle.IsInvalid -or $processHandle.IsClosed) {
                $cleanupIncomplete = $true
                if ($name -eq "VigilanTune.Watchdog") { $watchdogUnresolved = $true }
                Write-Warning "Skipped process $($process.Id): its process handle could not be verified."
                continue
            }
            $processPath = $process.Path
        } catch {
            $cleanupIncomplete = $true
            if ($name -eq "VigilanTune.Watchdog") { $watchdogUnresolved = $true }
            Write-Warning "Skipped process $($process.Id): its executable path could not be verified."
            continue
        }
        if ([string]::IsNullOrWhiteSpace($processPath)) {
            $cleanupIncomplete = $true
            if ($name -eq "VigilanTune.Watchdog") { $watchdogUnresolved = $true }
            Write-Warning "Skipped process $($process.Id): its executable path could not be verified."
            continue
        }
        $status = Get-InstalledExecutableStatus -Path $processPath -ExpectedName "$name.exe"
        if ($status -eq "unrelated" -and $name -ne "VigilanTune.Watchdog") {
            Write-Host "Skipped unrelated process $($process.Id)."
            continue
        }
        if ($status -ne "verified") {
            $cleanupIncomplete = $true
            if ($name -eq "VigilanTune.Watchdog") { $watchdogUnresolved = $true }
            Write-Warning "Skipped process $($process.Id): it is not a verified executable in this installation."
            continue
        }
        try {
            Stop-Process -InputObject $process -Force -ErrorAction Stop
            if (-not $process.WaitForExit(5000)) {
                throw "Timed out waiting for the process to exit."
            }
            Write-Host "Stopped $name (PID $($process.Id))." -ForegroundColor Green
        } catch {
            $cleanupIncomplete = $true
            if ($name -eq "VigilanTune.Watchdog") { $watchdogUnresolved = $true }
            Write-Warning "Could not stop $name (PID $($process.Id)): $_"
        }
    }
}

Write-Host "No power-plan or telemetry cleanup was attempted."
if ($cleanupIncomplete) {
    throw "One or more matching processes could not be verified or stopped."
}
