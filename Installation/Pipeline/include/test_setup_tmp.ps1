New-Item -ItemType Directory -Force -Path log-output
New-Item -ItemType Directory -Force -Path tmp

$tmp=(Get-Location).path

$env:TEMP="$tmp\tmp"
$env:TMP="$tmp\tmp"
$env:TMPDIR="$tmp\tmp"
$env:TEMPDIR="$tmp\tmp"
