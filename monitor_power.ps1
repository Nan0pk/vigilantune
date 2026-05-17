$logFile = "C:\Users\Victus\power_usage.log"
"Monitoring started at $(Get-Date) - Includes Process & System Settings" | Out-File $logFile
while ($true) {
    $battery = Get-CimInstance -ClassName Win32_Battery
    $status = Get-CimInstance -Namespace root/wmi -ClassName BatteryStatus
    $percentage = $battery.EstimatedChargeRemaining
    $online = $status.PowerOnline
    $rate = $status.DischargeRate
    $time = Get-Date -Format "HH:mm:ss"
    
    # System Settings
    $brightness = (Get-CimInstance -Namespace root/WMI -ClassName WmiMonitorBrightness -ErrorAction SilentlyContinue).CurrentBrightness
    $schemeInfo = powercfg /getactivescheme
    $scheme = if ($schemeInfo -match "\((.*)\)") { $matches[1] } else { "Unknown" }
    $cpuSpeed = (Get-CimInstance -ClassName Win32_Processor).CurrentClockSpeed
    
    # Top 5 CPU consuming processes
    $topProcs = Get-Process | Sort-Object CPU -Descending | Select-Object -First 5 | ForEach-Object { "$($_.Name) ($([math]::Round($_.CPU, 1))s)" }
    $procList = $topProcs -join ", "

    $msg = "[$time] Bat: $percentage% | Rate: $rate mW | Online: $online"
    $msg2 = "       Settings: Brightness: $brightness% | Scheme: $scheme | CPU: $cpuSpeed MHz"
    $msg3 = "       Top CPU: $procList"
    
    $msg | Out-File $logFile -Append
    $msg2 | Out-File $logFile -Append
    $msg3 | Out-File $logFile -Append
    
    if ($online -eq $true) {
        "[$time] AC Power detected. Monitoring stopped." | Out-File $logFile -Append
        break
    }
    Start-Sleep -Seconds 60
}
