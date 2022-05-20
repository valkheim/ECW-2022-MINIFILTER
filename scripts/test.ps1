.\unload.ps1

Write-Host "[+] Prepare secret directory and file" -ForegroundColor Green
Remove-Item -Path .\secret\ -Force -Recurse -Confirm:$false
New-Item -Path .\secret\ -ItemType Directory -Force
"This will be written to the text file" | Out-File -FilePath  .\secret\file.txt

.\load.ps1

Write-Host "[+] Appending line to secret file" -ForegroundColor Green
"This will be appended beneath the first line" | Out-File -FilePath .\secret\file.txt -Append

.\unload.ps1