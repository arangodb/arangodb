param(
[int] $AgentCount = 1,
[int] $CoordinatorCount = 1,
[int] $DBServerCount = 2,
[int] $AgentStartPort = 4001,
[int] $CoordinatorStartPort = 8530,
[int] $DBServerStartPort = 8629,
[string] $JwtSecret = ""
)

$commonArguments = @(
    "-c", "none",
    "--javascript.app-path=.\js\apps",
    "--javascript.startup-directory=.\js",
    "--javascript.module-directory=.\enterprise\js",
    "--log.force-direct=true"
)
if ($JwtSecret) {
    $commonArguments += "--server.authentication=true","--server.jwt-secret=$JwtSecret"
} else {
    $commonArguments += "--server.authentication=false"
}


$ports=@()
Remove-Item -Recurse -Force cluster | Out-Null
New-Item -Path cluster -Force -ItemType Directory | Out-Null

$agentArguments = @(
    "--agency.activate=true",
    "--agency.supervision=true",
    "--agency.size=$AgentCount",
    "--javascript.v8-contexts=1",
    "--server.statistics=false",
    "--agency.endpoint=tcp://[::1]:$AgentStartPort"
)

$processes = @()
$agentArguments += $commonArguments
for ($i=0;$i -lt $AgentCount;$i++) {
    $port = $AgentStartPort + $i
    $ports+=$port
    Write-Host "Starting AGENT on $port"
    $arguments = @(
        "--agency.my-address=tcp://[::1]:$port",
        "--database.directory=cluster\data$port",
        "--log.file=cluster\$port.log",
        "--server.endpoint=tcp://[::1]:$port"
    )
    $arguments += $agentArguments
    $processes += Start-Process `
        -RedirectStandardError "cluster\$port.err" `
        -RedirectStandardOutput "cluster\$port.out" `
        -FilePath .\build\bin\arangod.exe `
        -ArgumentList $arguments `
        -NoNewWindow `
        -PassThru
}

$map=@{
    "COORDINATOR"= @{
        "count"= $CoordinatorCount;
        "startport"= $CoordinatorStartPort
    }
    "DBSERVER"= @{
        "count"= $DBServerCount;
        "startport"= $DBServerStartPort
    }
}

foreach ($it in $map.GetEnumerator()) {
    $role = $it.Key
    $struct = $it.Value
    for ($i=0;$i -lt $struct.count;$i++) {
        $port = $struct.startport + $i
        $ports+=$port

        $roleArguments = @(
            "--cluster.agency-endpoint=tcp://[::1]:$AgentStartPort",
            "--cluster.my-role=$role",
            "--cluster.my-address=tcp://[::1]:$port",
            "--database.directory=cluster\data$port",
            "--server.endpoint=tcp://[::1]:$port",
            "--log.file=cluster\$port.log"
        )
        $roleArguments += $commonArguments

        Write-Host "Starting $role on $port"
        $processes += Start-Process `
            -RedirectStandardError "cluster\$port.err" `
            -RedirectStandardOutput "cluster\$port.out" `
            -FilePath .\build\bin\arangod.exe `
            -ArgumentList $roleArguments `
            -NoNewWindow `
            -PassThru
    }
}

while ($ports -gt 0) {
    $newPorts=@()
    foreach ($port in $ports) {
        try {
            $InvokeWebRequestArgs = @{
                "TimeoutSec"=1;
                "UseBasicParsing"=true;
                "URI"="http://[::1]:$port/_api/version";
            }
            if ($JwtSecret) {
                $InvokeWebRequestArgs["Headers"] = @{"Authorization"="bearer $(jwtgen -a HS256 -s $JwtSecret -c 'iss=arangodb' -c 'server_id=setup')"}
            }
            $req=Invoke-WebRequest @InvokeWebRequestArgs
            Write-Host "$port became ready!"
        } catch {
            $newPorts += $port;
        }
    }
    $ports=$newPorts
    if ($ports -gt 0) {
        Write-Host -NoNewline "."
        Start-Sleep -Seconds 1
    }
}

Write-Host ""
Write-Host "ArangoDB Cluster is ready!"
Write-Host "Connect using an arangosh:"
Write-Host ""
Write-Host "    .\build\bin\arangosh --server.endpoint=tcp://[::1]:$CoordinatorStartPort"
