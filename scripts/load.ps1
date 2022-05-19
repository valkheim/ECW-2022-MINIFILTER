# from elevated powershell in Z: or \\10.0.2.4\qemu
rundll32.exe setupapi.dll,InstallHinfSection DefaultInstall 132 ..\build\x64\kapp\Debug\kapp.inf
fltMC.exe load kapp