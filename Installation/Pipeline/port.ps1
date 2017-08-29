echo "HMM"

New-Item -ItemType Directory -Force -Path C:\ports | Out-Null

echo "HUU"

$timeLimit = (Get-Date).AddMinutes(-480)
Get-ChildItem C:\ports | ? { $_.LastWriteTime -lt $timeLimit } | Remove-Item -ErrorAction SilentlyContinue

$port = 10000
$portIncrement = 2000

$port = $port - $portIncrement

do {
    $port = $port + $portIncrement
    $portFile = "C:\ports\$port"
}
until (New-Item -ItemType File -Path $portFile -ErrorAction SilentlyContinue)
Write-Output $port