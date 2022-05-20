# from elevated powershell in Z: or \\10.0.2.4\qemu
Write-Host "[+] Installing kapp" -ForegroundColor Green
rundll32.exe setupapi.dll,InstallHinfSection DefaultInstall 132 C:\workspace\ecw_minifilter\build\x64\kapp\Debug\kapp.inf
Start-Sleep -Seconds 0.7
Write-Host "[+] Loading kapp" -ForegroundColor Green
fltMC.exe load kapp
Start-Sleep -Seconds 0.7
fltMC.exe