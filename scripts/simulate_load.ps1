# nanoloop Load Simulation Script
# Purpose: Empirically validate agent response to CPU and Thread backlog spikes

param (
    [int]$DurationSeconds = 30,
    [int]$SpikeIntensity = 10 # Number of concurrent worker threads
)

Write-Host "--- nanoloop Load Simulator ---" -ForegroundColor Cyan
Write-Host "Target: Spiking CPU and Thread Queue for $DurationSeconds seconds..."

$stopTime = (Get-Date).AddSeconds($DurationSeconds)

$jobs = for ($i = 0; $i -lt $SpikeIntensity; $i++) {
    Start-Job -ScriptBlock {
        $endTime = [DateTime]::Parse($using:stopTime)
        while ((Get-Date) -lt $endTime) {
            # Tight loop to consume CPU cycles
            $x = 1.1
            for ($j = 0; $j -lt 10000; $j++) { $x *= 1.1 }
        }
    }
}

Write-Host "Load active. Monitor the nanoloop Agent log for SSS spikes." -ForegroundColor Yellow

while ((Get-Date) -lt $stopTime) {
    $timeLeft = ($stopTime - (Get-Date)).Seconds
    Write-Progress -Activity "Simulating Load" -Status "$timeLeft seconds remaining" -PercentComplete ((1 - ($timeLeft / $DurationSeconds)) * 100)
    Start-Sleep -Seconds 1
}

Write-Host "Load simulation complete. Cleaning up..." -ForegroundColor Green
Get-Job | Stop-Job
Receive-Job -Job $jobs
Remove-Job -Job $jobs
