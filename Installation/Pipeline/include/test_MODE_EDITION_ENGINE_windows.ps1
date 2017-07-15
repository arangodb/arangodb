WorkFlow RunTests {
  Param ([int]$port, [string]$engine, [string]$edition, [string]$mode)

  $minPort = $port
  $workspace = Get-Location

  if ($mode -eq "singleserver") {
    $portInterval = 10
    $cluster = "false"

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
  }
  else {
    $portInterval = 40
    $cluster = "true"

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
  }

  foreach -parallel -throttlelimit 5 ($testdef in $tests) {
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

        echo "Starting $USING:name"

        Set-Location $USING:workspace
        .\build\bin\arangosh.exe --log.level warning --javascript.execute UnitTests\unittest.js $USING:test -- --cluster $USING:cluster --storageEngine $USING:engine --minPort $USING:minPort --maxPort $USING:maxPort --skipNondeterministic true --skipTimeCritical true  --configDir etc/jenkins --skipLogAnalysis true $USING:testargs *> $USING:log

        echo "Finished $USING:name"
      }

      Invoke-Command -ScriptBlock $testscript
    }

    $WORKFLOW:minPort=$WORKFLOW:minPort+$portInterval
  }
}
