.\unload.ps1

Write-Host "[+] Prepare secret directory and file" -ForegroundColor Green
Remove-Item -Path .\secret\ -Force -Recurse -Confirm:$false
New-Item -Path .\secret\ -ItemType Directory -Force
"ECW{this_is_the_flag}" | Out-File -FilePath  .\secret\file.txt
"ECW{this_is_the_flag}" | Out-File -FilePath  .\secret\original.txt

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