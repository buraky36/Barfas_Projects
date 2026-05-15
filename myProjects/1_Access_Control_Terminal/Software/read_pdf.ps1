$filePath = 'C:\Users\hp\Desktop\Barfas\myProjects\1_Access_Control_Terminal\1_Access_Terminal_Control\Docs\XR1300V1.10_1.pdf'
$bytes = [System.IO.File]::ReadAllBytes($filePath)
# Convert bytes to chars, replacing non-printable with space
$chars = $bytes | ForEach-Object { if ($_ -ge 32 -and $_ -le 126) { [char]$_ } else { ' ' } }
$content = -join $chars
# Split on multiple spaces to get tokens
$tokens = $content -split '\s{3,}'
Write-Host "Total tokens: $($tokens.Count)"
Write-Host "=== Tokens matching keywords ==="
$tokens | Where-Object { $_ -match '1000|Continuous|Trigger|Scan|Baud|Auto|Mode|checksum|register|\$10' } | Select-Object -First 100 | ForEach-Object { Write-Host ">>> $_" }
