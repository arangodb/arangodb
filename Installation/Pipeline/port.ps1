New-Item -ItemType Directory -Force -Path C:\ports

$timeLimit = (Get-Date).AddMinutes(-480)
Get-ChildItem C:\ports | ? { $_.LastWriteTime -lt $timeLimit } | Remove-Item

$port = 10000
$portIncrement = 2000

$port = $port - $portIncrement

do {
    $port = $port + $portIncrement
    $portFile = "C:\ports\$port"
}
until (New-Item -ItemType File -Path $portFile -ErrorAction SilentlyContinue)
Write-Output $port