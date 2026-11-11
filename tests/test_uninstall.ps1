$ErrorActionPreference = "Stop"

$testRoot = Join-Path (Get-Location).Path ("uninstall-safety-" + [guid]::NewGuid().ToString("N") + ".tmp")
$installRoot = Join-Path $testRoot "install"
$outsideRoot = Join-Path $testRoot "install-sibling"
$global:vigilantuneTestProcesses = @()
$global:vigilantuneTestStoppedIds = [System.Collections.Generic.List[int]]::new()
$global:vigilantuneTestStopFailures = [System.Collections.Generic.HashSet[int]]::new()
$global:vigilantuneTestPowerCalls = [System.Collections.Generic.List[string]]::new()
$global:vigilantuneTestPlanGuid = [guid]::NewGuid().ToString()
$global:vigilantuneTestEnumerationCount = 0
$global:vigilantuneTestFailEnumerationAt = 0
$global:vigilantuneTestLateWatchdog = $null

function Get-Process {
    [CmdletBinding()]
    param([string]$Name)
    $global:vigilantuneTestEnumerationCount++
    if ($global:vigilantuneTestEnumerationCount -eq $global:vigilantuneTestFailEnumerationAt) {
        throw "Synthetic process enumeration failure"
    }
    $active = @($global:vigilantuneTestProcesses | Where-Object {
        -not ($global:vigilantuneTestStoppedIds.Contains([int]$_.Id) -and $_.Exits)
    })
    if ($null -ne $global:vigilantuneTestLateWatchdog -and $global:vigilantuneTestEnumerationCount -ge 2) {
        $active += $global:vigilantuneTestLateWatchdog
    }
    if ($PSBoundParameters.ContainsKey("Name")) {
        $active | Where-Object { $_.Name -ieq $Name }
    } else {
        $active
    }
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

function New-FixtureProcess {
    param([string]$Name, [int]$Id, [AllowNull()][object]$Path, [object]$Handle, [bool]$Exits = $true)
    $fixture = [pscustomobject]@{ Name = $Name; ProcessName = $Name; Id = $Id; Path = $Path; SafeHandle = $Handle; Exits = $Exits }
    $fixture | Add-Member -MemberType ScriptMethod -Name WaitForExit -Value {
        param([int]$Milliseconds)
        return $this.Exits
    }
    return $fixture
}

function Test-TelemetryFixture {
    param([string]$Path)
    return (Test-Path -LiteralPath $Path) -and ([System.IO.File]::ReadAllText($Path) -ceq "synthetic user data")
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
    $outsideWatchdog = Join-Path $outsideRoot "VigilanTune.Watchdog.exe"
    foreach ($path in @($agent, $watchdog, $unrelated, $outside, $outsideWatchdog)) {
        [System.IO.File]::WriteAllText($path, "synthetic fixture")
    }
    $telemetry = Join-Path $models "telemetry_log.csv"
    [System.IO.File]::WriteAllText($telemetry, "synthetic user data")

    $validHandle = [pscustomobject]@{ IsInvalid = $false; IsClosed = $false }
    $invalidHandle = [pscustomobject]@{ IsInvalid = $true; IsClosed = $false }
    $closedHandle = [pscustomobject]@{ IsInvalid = $false; IsClosed = $true }
    $global:vigilantuneTestProcesses = @(
        New-FixtureProcess -Name "VigilanTune.Watchdog" -Id 11 -Path $watchdog -Handle $validHandle
        New-FixtureProcess -Name "VigilanTune.Agent" -Id 12 -Path $agent -Handle $validHandle
        New-FixtureProcess -Name "VigilanTune.Agent" -Id 21 -Path $outside -Handle $validHandle
        New-FixtureProcess -Name "VigilanTune.Agent" -Id 22 -Path $unrelated -Handle $validHandle
        New-FixtureProcess -Name "agent" -Id 31 -Path $outside -Handle $validHandle
    )
    $unverifiedProcesses = @(
        New-FixtureProcess -Name "VigilanTune.Agent" -Id 23 -Path $null -Handle $validHandle
        New-FixtureProcess -Name "VigilanTune.Agent" -Id 25 -Path $agent -Handle $invalidHandle
        New-FixtureProcess -Name "VigilanTune.Agent" -Id 26 -Path $agent -Handle $closedHandle
    )
    $caseSibling = Join-Path $testRoot "INSTALL"
    if (-not (Test-Path -LiteralPath $caseSibling)) {
        New-Item -ItemType Directory -Path $caseSibling | Out-Null
        $caseSiblingAgent = Join-Path $caseSibling "VigilanTune.Agent.exe"
        [System.IO.File]::WriteAllText($caseSiblingAgent, "synthetic fixture")
        $unverifiedProcesses += New-FixtureProcess -Name "VigilanTune.Agent" -Id 27 -Path $caseSiblingAgent -Handle $validHandle
    }

    $link = Join-Path $binaries "linked-agent/VigilanTune.Agent.exe"
    New-Item -ItemType Directory -Path (Split-Path -Parent $link) -Force | Out-Null
    try {
        New-Item -ItemType SymbolicLink -Path $link -Target $outside -ErrorAction Stop | Out-Null
        $unverifiedProcesses += New-FixtureProcess -Name "VigilanTune.Agent" -Id 24 -Path $link -Handle $validHandle
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
    if (-not (Test-TelemetryFixture -Path $telemetry)) {
        $failures.Add("Uninstall changed user telemetry without consent")
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
    if ($global:vigilantuneTestPowerCalls.Count -ne 0 -or -not (Test-TelemetryFixture -Path $telemetry)) {
        $failures.Add("Stop failure affected power plans or user telemetry")
    }

    $global:vigilantuneTestStopFailures.Clear()
    $global:vigilantuneTestStoppedIds.Clear()
    $global:vigilantuneTestProcesses += $unverifiedProcesses
    $verificationFailureReported = $false
    try {
        & (Join-Path $scripts "uninstall.ps1")
    } catch {
        $verificationFailureReported = $true
    }
    if (-not $verificationFailureReported) {
        $failures.Add("Uninstall reported success when process ownership could not be verified")
    }
    if (($global:vigilantuneTestStoppedIds | Sort-Object) -join "," -ne "11,12") {
        $failures.Add("Unverified or unrelated process was stopped")
    }
    if ($global:vigilantuneTestPowerCalls.Count -ne 0 -or -not (Test-TelemetryFixture -Path $telemetry)) {
        $failures.Add("Verification failure affected power plans or user telemetry")
    }

    $global:vigilantuneTestProcesses = @(
        New-FixtureProcess -Name "VigilanTune.Watchdog" -Id 11 -Path $watchdog -Handle $validHandle
        New-FixtureProcess -Name "VigilanTune.Agent" -Id 12 -Path $agent -Handle $validHandle
    )
    $global:vigilantuneTestStoppedIds.Clear()
    [void]$global:vigilantuneTestStopFailures.Add(11)
    $watchdogFailureReported = $false
    try {
        & (Join-Path $scripts "uninstall.ps1")
    } catch {
        $watchdogFailureReported = $true
    }
    if (-not $watchdogFailureReported -or $global:vigilantuneTestStoppedIds.Count -ne 0) {
        $failures.Add("An agent was stopped despite a watchdog stop failure")
    }

    $global:vigilantuneTestStopFailures.Clear()
    $global:vigilantuneTestStoppedIds.Clear()
    $global:vigilantuneTestProcesses = @(
        New-FixtureProcess -Name "VigilanTune.Watchdog" -Id 41 -Path $watchdog -Handle $invalidHandle
        New-FixtureProcess -Name "VigilanTune.Agent" -Id 12 -Path $agent -Handle $validHandle
    )
    $watchdogVerificationReported = $false
    try {
        & (Join-Path $scripts "uninstall.ps1")
    } catch {
        $watchdogVerificationReported = $true
    }
    if (-not $watchdogVerificationReported -or $global:vigilantuneTestStoppedIds.Count -ne 0) {
        $failures.Add("An agent was stopped despite an unverified watchdog")
    }

    $global:vigilantuneTestStoppedIds.Clear()
    $global:vigilantuneTestProcesses = @(
        New-FixtureProcess -Name "VigilanTune.Watchdog" -Id 11 -Path $watchdog -Handle $validHandle -Exits $false
        New-FixtureProcess -Name "VigilanTune.Agent" -Id 12 -Path $agent -Handle $validHandle
    )
    $watchdogTimeoutReported = $false
    try {
        & (Join-Path $scripts "uninstall.ps1")
    } catch {
        $watchdogTimeoutReported = $true
    }
    if (-not $watchdogTimeoutReported -or ($global:vigilantuneTestStoppedIds -join ",") -ne "11") {
        $failures.Add("An agent was stopped before the watchdog exited")
    }

    $global:vigilantuneTestStoppedIds.Clear()
    $global:vigilantuneTestProcesses = @(
        New-FixtureProcess -Name "VigilanTune.Watchdog" -Id 42 -Path $outsideWatchdog -Handle $validHandle
        New-FixtureProcess -Name "VigilanTune.Agent" -Id 12 -Path $agent -Handle $validHandle
    )
    $otherWatchdogReported = $false
    try {
        & (Join-Path $scripts "uninstall.ps1")
    } catch {
        $otherWatchdogReported = $true
    }
    if (-not $otherWatchdogReported -or $global:vigilantuneTestStoppedIds.Count -ne 0) {
        $failures.Add("An agent was stopped while an unrelated watchdog might still monitor it")
    }

    $global:vigilantuneTestStoppedIds.Clear()
    $global:vigilantuneTestEnumerationCount = 0
    $global:vigilantuneTestProcesses = @(
        New-FixtureProcess -Name "VigilanTune.Agent" -Id 12 -Path $agent -Handle $validHandle
    )
    $global:vigilantuneTestLateWatchdog = New-FixtureProcess -Name "VigilanTune.Watchdog" -Id 43 -Path $outsideWatchdog -Handle $validHandle
    $newWatchdogReported = $false
    try {
        & (Join-Path $scripts "uninstall.ps1")
    } catch {
        $newWatchdogReported = $true
    }
    if (-not $newWatchdogReported -or $global:vigilantuneTestStoppedIds.Count -ne 0) {
        $failures.Add("An agent was stopped after a new watchdog appeared")
    }
    $global:vigilantuneTestLateWatchdog = $null

    foreach ($failureAt in @(1, 2)) {
        $global:vigilantuneTestStoppedIds.Clear()
        $global:vigilantuneTestEnumerationCount = 0
        $global:vigilantuneTestFailEnumerationAt = $failureAt
        $global:vigilantuneTestProcesses = @(
            New-FixtureProcess -Name "VigilanTune.Watchdog" -Id 11 -Path $watchdog -Handle $validHandle
            New-FixtureProcess -Name "VigilanTune.Agent" -Id 12 -Path $agent -Handle $validHandle
        )
        $enumerationFailureReported = $false
        try {
            & (Join-Path $scripts "uninstall.ps1")
        } catch {
            $enumerationFailureReported = $true
        }
        $expectedIds = if ($failureAt -eq 1) { "" } else { "11" }
        if (-not $enumerationFailureReported -or ($global:vigilantuneTestStoppedIds -join ",") -ne $expectedIds) {
            $failures.Add("Process enumeration failure did not stop cleanup safely")
        }
    }
    $global:vigilantuneTestFailEnumerationAt = 0
    if ($global:vigilantuneTestPowerCalls.Count -ne 0 -or -not (Test-TelemetryFixture -Path $telemetry)) {
        $failures.Add("Watchdog failures affected power plans or user telemetry")
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
