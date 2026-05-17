# [WinSCADA] Power Monitoring Script
# Corrected for portability and accuracy

$logFile = Join-Path $env:USERPROFILE "power_usage.log"
Write-Host "Monitoring power usage. Logging to: $logFile"

while ($true) {
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    
    # Get current CPU Load Percentage (not cumulative seconds)
    $cpu = Get-WmiObject Win32_Processor | Measure-Object -Property LoadPercentage -Average | Select-Object -ExpandProperty Average
    
    # Get Battery Status (if available)
    $battery = Get-WmiObject -Class "Win32_Battery" -ErrorAction SilentlyContinue
    $status = if ($battery) { "$($battery.EstimatedChargeRemaining)%" } else { "AC Power" }
    
    $entry = "$timestamp | CPU: $cpu% | Battery: $status"
    Write-Host $entry
    $entry | Out-File -FilePath $logFile -Append
    
    Start-Sleep -Seconds 5
}
