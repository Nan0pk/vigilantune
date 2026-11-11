param(
    [Parameter(Mandatory = $true)][string]$AgentFixture,
    [Parameter(Mandatory = $true)][string]$WatchdogFixture
)

$ErrorActionPreference = "Stop"
$testRoot = Join-Path (Get-Location).Path ("uninstall-live-" + [guid]::NewGuid().ToString("N") + ".tmp")
$installRoot = Join-Path $testRoot "install"
$started = @()
$skip = $false

if (@(Get-Process -Name "VigilanTune.Watchdog" -ErrorAction SilentlyContinue).Count -gt 0) {
    Write-Host "Skipping the live fixture because another VigilanTune watchdog is running."
    exit 77
}

try {
    $scripts = Join-Path $installRoot "scripts"
    $binaries = Join-Path $installRoot "bin"
    New-Item -ItemType Directory -Path $scripts, $binaries -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot "../scripts/uninstall.ps1") -Destination $scripts
    $agentPath = Join-Path $binaries "VigilanTune.Agent.exe"
    $watchdogPath = Join-Path $binaries "VigilanTune.Watchdog.exe"
    Copy-Item -LiteralPath $AgentFixture -Destination $agentPath
    Copy-Item -LiteralPath $WatchdogFixture -Destination $watchdogPath

    $watchdog = Start-Process -FilePath $watchdogPath -PassThru -WindowStyle Hidden
    $started += $watchdog
    $watchdogHandle = $watchdog.SafeHandle
    $agent = Start-Process -FilePath $agentPath -PassThru -WindowStyle Hidden
    $started += $agent
    $agentHandle = $agent.SafeHandle
    if ($watchdog.HasExited -or $agent.HasExited) {
        throw "A synthetic process exited before cleanup began."
    }

    try {
        & (Join-Path $scripts "uninstall.ps1")
    } catch {
        $otherWatchdogs = @(Get-Process -Name "VigilanTune.Watchdog" -ErrorAction SilentlyContinue |
            Where-Object { $_.Id -ne $watchdog.Id })
        if ($otherWatchdogs.Count -eq 0) {
            throw
        }
        Write-Host "Skipping the live fixture because another VigilanTune watchdog started."
        $skip = $true
    }
    if (-not $skip) {
        if (-not $watchdog.WaitForExit(5000) -or -not $agent.WaitForExit(5000)) {
            throw "Cleanup did not stop both synthetic processes."
        }
        Write-Host "Cleanup stopped both owned synthetic processes."
    }
} finally {
    $allStopped = $true
    foreach ($process in $started) {
        try {
            if (-not $process.HasExited) {
                $process.Kill()
                if (-not $process.WaitForExit(5000)) {
                    $allStopped = $false
                }
            }
        } catch {
            $allStopped = $false
            Write-Warning "Could not stop synthetic process $($process.Id): $_"
        }
    }
    if ($allStopped -and (Test-Path -LiteralPath $testRoot)) {
        Remove-Item -LiteralPath $testRoot -Recurse -Force
    }
    if (-not $allStopped) {
        throw "A synthetic process could not be stopped; test files were retained at $testRoot."
    }
}
if ($skip) {
    exit 77
}
