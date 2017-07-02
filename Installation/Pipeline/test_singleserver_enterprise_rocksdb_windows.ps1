New-Item -ItemType Directory -Force -Path C:\ports
New-Item -ItemType Directory -Force -Path log-output
New-Item -ItemType Directory -Force -Path tmp

$env:TEMP=".\tmp"

$timeLimit = (Get-Date).AddDays(-1) 
Get-ChildItem C:\ports | ? { $_.PSIsContainer -and $_.LastWriteTime -lt $timeLimit } | Remove-Item

$port = 15000
$portIncrement = 2000

$port = $port - $portIncrement

do {
    $port = $port + $portIncrement
    $portFile = "C:\ports\$port"
}
until (New-Item -ItemType File -Path $portFile -ErrorAction SilentlyContinue)

WorkFlow RunTests {
  Param ([int]$port)

  $minPort = $port
  $portInterval = 10
  $workspace = Get-Location

  $tests = @(
    "agency",
    @("boost", "boost", "--skipCache false"),
    "arangobench",
    "arangosh",
    "authentication",
    "authentication_parameters",
    "cluster_sync",
    "config",
    "dfdb",
    "dump",
    "dump_authentication",
    "endpoints",
    @("http_replication","http_replication", "--rspec C:\tools\ruby23\bin\rspec.bat"),
    @("http_server","http_server", "--rspec C:\tools\ruby23\bin\rspec.bat"),
    "replication_sync",
    "replication_static",
    "replication_ongoing",
    "server_http",
    "shell_client",
    "shell_replication",
    "shell_server",
    @("shell_server_aql_1", "shell_server_aql","--testBuckets 4/0"),
    @("shell_server_aql_2", "shell_server_aql","--testBuckets 4/1"),
    @("shell_server_aql_3", "shell_server_aql","--testBuckets 4/2"),
    @("shell_server_aql_4", "shell_server_aql","--testBuckets 4/3"),
    @("ssl_server","ssl_server", "--rspec C:\tools\ruby23\bin\rspec.bat"),
    "upgrade"
  )

  foreach -parallel -throttlelimit 3 ($testdef in $tests) {
    $testargs = ""

    if ($testdef -isnot [system.array]) {
      $name = $testdef
      $test = $testdef
    } else {
      $name = $testdef[0]
      $test = $testdef[1]
      $testargs = $testdef[2].Split(" ")
    }

    $log = "log-output\" + $name + ".log"

    InlineScript {
      $testscript = {
        $maxPort = $USING:minPort + $USING:portInterval - 1

        Set-Location $USING:workspace
        Start-Transcript -Path $USING:log
        .\build\bin\arangosh.exe --log.level warning --javascript.execute UnitTests\unittest.js $USING:test -- --storageEngine rocksdb --minPort $USING:minPort --maxPort $USING:maxPort --skipNondeterministic true --skipTimeCritical true  --configDir etc/jenkins --skipLogAnalysis true $USING:testargs
        Stop-Transcript
      }

      Invoke-Command -ScriptBlock $testscript
    }

    $WORKFLOW:minPort=$WORKFLOW:minPort+$portInterval
  }
}

move .\build\bin\RelWithDebInfo\* .\build\bin\

RunTests -port $port
$result = $LastExitCode

del $portFile

exit $result
