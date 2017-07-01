New-Item -ItemType Directory -Force -Path C:\ports

$port = 15000
$portIncrement = 2000

$port = $port - $portIncrement

do {
    $port = $port + $portIncrement
    $portFile = "C:\ports\$port"
}
until (New-Item -ItemType File -Path $portFile -ErrorAction SilentlyContinue)

WorkFlow RunTests {
  $minPort = $using:port
  $portInterval = 40
  $tests = @(
    "arangobench",
    "arangosh",
    "authentication",
    "authentication_parameters",
    "config",
    "dump",
    "dump_authentication",
    "endpoints",
    @("http_server","http_server", "--rspec C:\tools\ruby23\bin\rspec.bat"),
    "server_http",
    "shell_client",
    "shell_server",
    @("shell_server_aql_1", "shell_server_aql","--testBuckets 4/0"),
    @("shell_server_aql_2", "shell_server_aql","--testBuckets 4/1"),
    @("shell_server_aql_3", "shell_server_aql","--testBuckets 4/2"),
    @("shell_server_aql_4", "shell_server_aql","--testBuckets 4/3"),
    @("ssl_server","ssl_server", "--rspec C:\tools\ruby23\bin\rspec.bat"),
    "upgrade"
  )
  $workspace=$ENV:WORKSPACE
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
    $log = $name + ".log"
    InlineScript {
      $testscript = {
        cd $USING:workspace
        # Start-Transcript -Path $USING:log
        .\build\bin\arangosh.exe --log.level warning --javascript.execute UnitTests\unittest.js $USING:test -- --cluster true --minPort $USING:minPort --maxPort $USING:minPort+portInterval-1 --skipNondeterministic true --skipTimeCritical true  --configDir etc/jenkins --skipLogAnalysis true $USING:testargs
        # Stop-Transcript
      }
      Invoke-Command -ScriptBlock $testscript
    }
    $WORKFLOW:minPort=$WORKFLOW:minPort+$portInterval
  }
}
New-Item -ItemType Directory -Force -Path tmp
$env:TEMP=".\tmp"
move .\build\bin\RelWithDebInfo\* .\build\bin\
RunTests
$result = $LastExitCode
del $portFile
exit $result
