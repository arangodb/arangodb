function executeParallel {
  Param(
    [System.Object[]]
    $jobs,
    [int]$parallelity
  )
  Get-Job | Remove-Job -Force | Out-Null

  $doneJobs = [System.Collections.ArrayList]$ArrayList = @()
  $numJobs = $jobs.Count

  $activeJobs = [System.Collections.ArrayList]$ArrayList = $()
  $index = 0
  $failed=$false
  while ($doneJobs.Count -lt $numJobs) {
    $activeJobs = Get-Job
    while ($index -lt $numJobs -and $activeJobs.Length -lt $parallelity) {
      $job = $jobs[$index++]
      Write-Host "Starting $($job.name)"
      $j = Start-Job -Init ([ScriptBlock]::Create("Set-Location '$pwd'")) -Name $job.name -ScriptBlock $job.script -ArgumentList $job.args
      $activeJobs = Get-Job
    }
    
    $finishedJobs = $activeJobs | Wait-Job -Any
    ForEach ($finishedJob in $finishedJobs) {
      Write-Host "Job $($finishedJob.Name) $($finishedJob.State)"
      Write-Host "========================"
      $finishedJob.childJobs[0].Output | Out-String
      if ($finishedJob.ChildJobs[0].State -eq 'Failed') {
        $failed=$true
        Write-Host $finishedJob.childJobs[0].JobStateInfo.Reason.Message -ForegroundColor Red
      }
    }
    $doneJobs += $finishedJobs
    $finishedJobs | Remove-Job
  }
  if ($failed -eq $true) {
    throw "Some jobs failed!"
  }
}


function createTests {
  Param ([int]$port, [string]$engine, [string]$edition, [string]$mode)
  $minPort = $port

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
      #"dump",
      #"dump_authentication",
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

  New-Item -Force log-output -type Directory | Out-Null 
  $createTestScript = {
    $testargs = ""

    if ($_ -isnot [system.array]) {
      $name = $_
      $test = $_
    } else {
      $name = $_[0]
      $test = $_[1]
      $testargs = $_[2].Split(" ")
    }

    $log = "log-output\" + $name + ".log"

    $myport = $minPort
    $minPort += $portInterval
    $maxPort = $minPort - 1 # minport was already increased

    return @{
      name=$name
      script={
        param($name, $myport, $maxPort, $test, $cluster, $engine, $testArgs, $log)
        # ridiculous...first allow it to continue because as soon as something will write to stderr it will fail
        # however some of these tests trigger these and actually some errors are to be expected.
        $ErrorActionPreference="SilentlyContinue"
        .\build\bin\arangosh.exe --log.level warning --javascript.execute UnitTests\unittest.js $test -- --cluster $cluster --storageEngine $engine --minPort $myport --maxPort $maxPort --skipNondeterministic true --skipTimeCritical true  --configDir etc/jenkins --skipLogAnalysis true $testargs *>&1 | Tee-Object -FilePath $log
        # $? will actually be false on those bogus "errors". however $LASTEXITCODE seems to always contain the real result we are interested in
        $result=$LASTEXITCODE
        # the only one who really knows if it broke or not is arangosh itself. so catch the error code
        # THEN REENABLE THE FCKING ERROR HANDLING
        $ErrorActionPreference="Stop"
        # and finally throw an error only if there really was an error
        if ($result -ne 0) {
          throw "arangosh returned a non zero exit code: $result!"
        }
      }
      args=@($name, $myport, $maxPort, $test, $cluster, $engine, $testArgs, $log)
    }
  }

  $tests | % $createTestScript
}
function RunTests {
  Param ([int]$port, [string]$engine, [string]$edition, [string]$mode)
  $jobs = createTests -port $port -engine $engine -edition $edition -mode $mode
  executeParallel -jobs $jobs -parallelity 4
}