Write-Host "[+] Unloading kapp" -ForegroundColor Green
fltMC.exe unload kapp
Start-Sleep -Seconds 0.7
Write-Host "[+] Uninstalling kapp" -ForegroundColor Green
#rundll32.exe setupapi.dll,InstallHinfSection DefaultUninstall 132 C:\workspace\ecw_minifilter\build\x64\kapp\Debug\kapp.inf
rundll32.exe setupapi.dll,InstallHinfSection DefaultUninstall 132 C:\workspace\ecw_minifilter\scripts\Debug\kapp.inf