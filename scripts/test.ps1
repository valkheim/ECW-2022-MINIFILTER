.\unload.ps1

Write-Host "[+] Prepare secret directory and file" -ForegroundColor Green
Remove-Item -Path .\secret\ -Force -Recurse -Confirm:$false
New-Item -Path .\secret\ -ItemType Directory -Force
"ECW{Fl7_pO5T0P_fINIsH3D_PR0C3S5inG}" | Out-File -FilePath  .\secret\file.txt
"ECW{Fl7_pO5T0P_fINIsH3D_PR0C3S5inG}" | Out-File -FilePath  .\secret\original.txt

Write-Host "[+] Clear file contents" -ForegroundColor Green
#Format-Hex -Path .\secret\file.txt
xxd .\secret\file.txt

.\load.ps1

Write-Host "[+] Appending line to secret file" -ForegroundColor Green
"This will be appended beneath the first line" | Out-File -FilePath .\secret\file.txt -Append

.\unload.ps1

Write-Host "[+] Lock file contents" -ForegroundColor Green
#Get-Item .\secret\file.txt.lock | Format-Hex
xxd .\secret\file.txt.lock