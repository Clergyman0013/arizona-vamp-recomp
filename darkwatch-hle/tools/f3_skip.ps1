# f3_skip.ps1 [-Skip "first last`n..."] [-Name shot]: find which draw makes a fault on screen.
# Writes drawskip.txt beside the Release exe (frame draw numbers as F3's frame.txt counts them, left out of every
# frame), waits for the game to read it, presses F3 in the game window and saves the capture's frame as
# captures\<Name>.png. The game must run with XBE_CAPTURE_DIR=captures and the player must stand still.
param([string]$Skip = "", [string]$Name = "shot")
$rel = Join-Path $PSScriptRoot "..\out\build\release"
Set-Content -Encoding ascii "$rel\drawskip.txt" $Skip
Start-Sleep -Milliseconds 1500  # read again every 30 frames
$before = (Get-ChildItem "$rel\captures" -Directory | Sort-Object LastWriteTime | Select-Object -Last 1).Name
$p = Get-Process -Name Darkwatch | Select-Object -First 1
$ws = New-Object -ComObject WScript.Shell
[void]$ws.AppActivate($p.Id)
Start-Sleep -Milliseconds 300
$ws.SendKeys("{F3}")
for ($i = 0; $i -lt 20; $i++) {
    Start-Sleep -Milliseconds 500
    $last = Get-ChildItem "$rel\captures" -Directory | Sort-Object LastWriteTime | Select-Object -Last 1
    if ($last.Name -ne $before -and (Test-Path "$($last.FullName)\final.bmp")) { break }
}
Start-Sleep -Milliseconds 800
Add-Type -AssemblyName System.Drawing
$img = [System.Drawing.Image]::FromFile("$($last.FullName)\final.bmp")
$img.Save("$rel\captures\$Name.png", [System.Drawing.Imaging.ImageFormat]::Png)
$img.Dispose()
"$Name <- $($last.Name) skip='$Skip'"
