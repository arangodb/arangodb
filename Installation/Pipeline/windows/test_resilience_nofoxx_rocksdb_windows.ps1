$ErrorActionPreference="Stop"

. Installation\Pipeline\include\test_setup_tmp.ps1
. Installation\Pipeline\port.ps1

cd resilience
npm install
New-Item -ItemType Directory -Force -Path build
New-Item -ItemType Directory -Force -Path build\bin
Move-Item ..\build\bin\RelWithDebInfo\* .\build\bin\ -Force
Move-Item ..\etc . -Force
Move-Item ..\js . -Force
.\build\bin\arangod --version
$env:RESILIENCE_ARANGO_BASEPATH="."
$env:multi='spec=- xunit=report.xml'
$env:BLUEBIRD_DEBUG=1
$env:MIN_PORT=$port
$env:MAX_PORT=$port + 1999
$env:PORT_OFFSET=10
$env:ARANGO_STORAGE_ENGINE="rocksdb"
$env:FORCE_WINDOWS_TTY="1"
# $env:LOG_IMMEDIATE="1"
# $env:ARANGO_EXTRA_ARGS="--log.level=cluster=trace --log.level=communication=trace --log.level=requests=debug"
npm run test-jenkins-windows -- (Get-ChildItem -Path .\test\ -Exclude *foxx*).Fullname
$result = $?

del $portFile

exit $result
