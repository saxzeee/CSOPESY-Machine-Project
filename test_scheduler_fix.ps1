#!/usr/bin/env powershell

Write-Host "Testing scheduler-start fix..."
Write-Host ""

# Start the emulator in background and send commands
$process = Start-Process -FilePath ".\emulator.exe" -PassThru -NoNewWindow -RedirectStandardInput -RedirectStandardOutput

# Send commands with delays
Start-Sleep -Seconds 1
$process.StandardInput.WriteLine("initialize")
Start-Sleep -Seconds 2
$process.StandardInput.WriteLine("scheduler-start")
Start-Sleep -Seconds 2
Write-Host "Scheduler started, now testing if commands work..."
$process.StandardInput.WriteLine("screen -ls")
Start-Sleep -Seconds 2
$process.StandardInput.WriteLine("scheduler-stop")
Start-Sleep -Seconds 1
$process.StandardInput.WriteLine("exit")
Start-Sleep -Seconds 1

if (!$process.HasExited) {
    Write-Host "Force stopping process..."
    $process.Kill()
}

Write-Host "Test completed."
