New-Item -ItemType Directory -Force -Path C:\ports | Out-Null

$timeLimit = (Get-Date).AddMinutes(-100)
Get-ChildItem C:\ports | ? { $_.LastWriteTime -lt $timeLimit } | Remove-Item -ErrorAction Ignore | Out-Null

$port = 10000
$portIncrement = 2000

$port = $port - $portIncrement
do {
    $port = $port + $portIncrement
    $portFile = "C:\ports\$port"
}
until (New-Item -ItemType File -Path $portFile -ErrorAction Ignore)
Write-Output $port