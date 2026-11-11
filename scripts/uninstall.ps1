# VigilanTune Power Agent — safe pre-alpha cleanup
# Stops only verified executables inside this installation. Power plans and
# telemetry remain untouched until ownership and restoration are implemented.

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$separators = [char[]]@([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)
$installRoot = ([System.IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))).TrimEnd($separators)
$installPrefix = $installRoot + [System.IO.Path]::DirectorySeparatorChar

function Test-InstalledExecutable {
    param([string]$Path, [string]$ExpectedName)

    try {
        $fullPath = [System.IO.Path]::GetFullPath($Path)
        if (-not $fullPath.StartsWith($installPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            return $false
        }
        if ([System.IO.Path]::GetFileName($fullPath) -ine $ExpectedName) {
            return $false
        }

        $entry = Get-Item -LiteralPath $fullPath -Force -ErrorAction Stop
        if ($entry -isnot [System.IO.FileInfo]) {
            return $false
        }
        while ($null -ne $entry) {
            if (($entry.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
                return $false
            }
            $entryPath = ([System.IO.Path]::GetFullPath($entry.FullName)).TrimEnd($separators)
            if ([string]::Equals($entryPath, $installRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
                return $true
            }
            if ($entry -is [System.IO.FileInfo]) {
                $entry = $entry.Directory
            } else {
                $entry = $entry.Parent
            }
        }
    } catch {
        return $false
    }
    return $false
}

Write-Host "Stopping processes belonging to this VigilanTune installation..." -ForegroundColor Cyan
$stopFailed = $false
foreach ($name in @("VigilanTune.Watchdog", "VigilanTune.Agent")) {
    foreach ($process in @(Get-Process -Name $name -ErrorAction SilentlyContinue)) {
        try {
            # Keep the process object tied to this OS process while checking
            # its path and stopping it, even if the process exits meanwhile.
            $processHandle = $process.SafeHandle
            if ($null -eq $processHandle -or $processHandle.IsInvalid -or $processHandle.IsClosed) {
                Write-Warning "Skipped process $($process.Id): its process handle could not be verified."
                continue
            }
            $processPath = $process.Path
        } catch {
            Write-Warning "Skipped process $($process.Id): its executable path could not be verified."
            continue
        }
        if ([string]::IsNullOrWhiteSpace($processPath) -or
            -not (Test-InstalledExecutable -Path $processPath -ExpectedName "$name.exe")) {
            Write-Warning "Skipped process $($process.Id): it is not a verified executable in this installation."
            continue
        }
        try {
            Stop-Process -InputObject $process -Force -ErrorAction Stop
            Write-Host "Stopped $name (PID $($process.Id))." -ForegroundColor Green
        } catch {
            $stopFailed = $true
            Write-Warning "Could not stop $name (PID $($process.Id)): $_"
        }
    }
}

Write-Host "Power plans and telemetry were left unchanged."
if ($stopFailed) {
    throw "One or more verified VigilanTune processes could not be stopped."
}
