$ErrorActionPreference = "Stop"

$testRoot = Join-Path (Get-Location).Path ("uninstall-safety-" + [guid]::NewGuid().ToString("N") + ".tmp")
$installRoot = Join-Path $testRoot "install"
$outsideRoot = Join-Path $testRoot "install-sibling"
$global:vigilantuneTestProcesses = @()
$global:vigilantuneTestStoppedIds = [System.Collections.Generic.List[int]]::new()
$global:vigilantuneTestStopFailures = [System.Collections.Generic.HashSet[int]]::new()
$global:vigilantuneTestPowerCalls = [System.Collections.Generic.List[string]]::new()
$global:vigilantuneTestPlanGuid = [guid]::NewGuid().ToString()

function Get-Process {
    [CmdletBinding()]
    param([string]$Name)
    $global:vigilantuneTestProcesses | Where-Object { $_.Name -ieq $Name }
}

function Stop-Process {
    [CmdletBinding()]
    param(
        [Parameter(ValueFromPipeline = $true)][object]$InputObject,
        [int]$Id,
        [switch]$Force
    )
    process {
        if ($PSBoundParameters.ContainsKey("InputObject")) {
            $targetId = [int]$InputObject.Id
        } elseif ($PSBoundParameters.ContainsKey("Id")) {
            $targetId = $Id
        } else {
            throw "Missing simulated process"
        }
        if ($global:vigilantuneTestStopFailures.Contains($targetId)) {
            throw "Synthetic process stop failure"
        }
        $global:vigilantuneTestStoppedIds.Add($targetId)
    }
}

function powercfg {
    $global:vigilantuneTestPowerCalls.Add(($args -join " "))
    if ($args[0] -eq "/getactivescheme") {
        "Power Scheme GUID: $global:vigilantuneTestPlanGuid (Balanced)"
    } elseif ($args[0] -eq "/list") {
        "Power Scheme GUID: $global:vigilantuneTestPlanGuid (Nanoloop AI Optimized)"
    }
}

try {
    $scripts = Join-Path $installRoot "scripts"
    $binaries = Join-Path $installRoot "bin"
    $models = Join-Path $installRoot "models"
    New-Item -ItemType Directory -Path $scripts, $binaries, $models, $outsideRoot -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot "../scripts/uninstall.ps1") -Destination $scripts

    $agent = Join-Path $binaries "VigilanTune.Agent.exe"
    $watchdog = Join-Path $binaries "VigilanTune.Watchdog.exe"
    $unrelated = Join-Path $binaries "Other.exe"
    $outside = Join-Path $outsideRoot "VigilanTune.Agent.exe"
    foreach ($path in @($agent, $watchdog, $unrelated, $outside)) {
        [System.IO.File]::WriteAllText($path, "synthetic fixture")
    }
    $telemetry = Join-Path $models "telemetry_log.csv"
    [System.IO.File]::WriteAllText($telemetry, "synthetic user data")

    $validHandle = [pscustomobject]@{ IsInvalid = $false; IsClosed = $false }
    $invalidHandle = [pscustomobject]@{ IsInvalid = $true; IsClosed = $false }
    $closedHandle = [pscustomobject]@{ IsInvalid = $false; IsClosed = $true }
    $global:vigilantuneTestProcesses = @(
        [pscustomobject]@{ Name = "VigilanTune.Watchdog"; Id = 11; Path = $watchdog; SafeHandle = $validHandle },
        [pscustomobject]@{ Name = "VigilanTune.Agent"; Id = 12; Path = $agent; SafeHandle = $validHandle },
        [pscustomobject]@{ Name = "VigilanTune.Agent"; Id = 21; Path = $outside; SafeHandle = $validHandle },
        [pscustomobject]@{ Name = "VigilanTune.Agent"; Id = 22; Path = $unrelated; SafeHandle = $validHandle },
        [pscustomobject]@{ Name = "VigilanTune.Agent"; Id = 23; Path = $null; SafeHandle = $validHandle },
        [pscustomobject]@{ Name = "VigilanTune.Agent"; Id = 25; Path = $agent; SafeHandle = $invalidHandle },
        [pscustomobject]@{ Name = "VigilanTune.Agent"; Id = 26; Path = $agent; SafeHandle = $closedHandle },
        [pscustomobject]@{ Name = "agent"; Id = 31; Path = $outside; SafeHandle = $validHandle }
    )

    $link = Join-Path $binaries "linked-agent/VigilanTune.Agent.exe"
    New-Item -ItemType Directory -Path (Split-Path -Parent $link) -Force | Out-Null
    try {
        New-Item -ItemType SymbolicLink -Path $link -Target $outside -ErrorAction Stop | Out-Null
        $global:vigilantuneTestProcesses += [pscustomobject]@{ Name = "VigilanTune.Agent"; Id = 24; Path = $link; SafeHandle = $validHandle }
    } catch {
        Write-Host "Symbolic-link fixture unavailable; path and sibling checks still run."
    }

    foreach ($name in @("Get-Process", "Stop-Process", "powercfg")) {
        if ((Get-Command $name).CommandType -ne "Function") {
            throw "Test double missing for $name"
        }
    }
    & (Join-Path $scripts "uninstall.ps1")

    $failures = [System.Collections.Generic.List[string]]::new()
    $actualIds = ($global:vigilantuneTestStoppedIds | Sort-Object) -join ","
    if ($actualIds -ne "11,12") {
        $failures.Add("Expected only the installation's agent and watchdog to stop; got: $actualIds")
    }
    if ($global:vigilantuneTestPowerCalls.Count -ne 0) {
        $failures.Add("Uninstall attempted power-plan operations: $($global:vigilantuneTestPowerCalls -join ', ')")
    }
    if (-not (Test-Path -LiteralPath $telemetry)) {
        $failures.Add("Uninstall removed user telemetry without consent")
    }

    $global:vigilantuneTestStoppedIds.Clear()
    [void]$global:vigilantuneTestStopFailures.Add(12)
    $stopFailureReported = $false
    try {
        & (Join-Path $scripts "uninstall.ps1")
    } catch {
        $stopFailureReported = $true
    }
    if (-not $stopFailureReported) {
        $failures.Add("Uninstall reported success when a verified process could not be stopped")
    }
    if (($global:vigilantuneTestStoppedIds -join ",") -ne "11") {
        $failures.Add("Unexpected process stopped while handling a stop failure")
    }
    if ($global:vigilantuneTestPowerCalls.Count -ne 0 -or -not (Test-Path -LiteralPath $telemetry)) {
        $failures.Add("Stop failure affected power plans or user telemetry")
    }
    if ($failures.Count -gt 0) {
        throw ($failures -join "`n")
    }
    Write-Host "Uninstaller preserves other processes, power plans, and user telemetry."
} finally {
    if (Test-Path -LiteralPath $testRoot) {
        Remove-Item -LiteralPath $testRoot -Recurse -Force
    }
}
