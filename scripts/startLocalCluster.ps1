param(
[int] $AgentCount = 1,
[int] $CoordinatorCount = 1,
[int] $DBServerCount = 2,
[int] $AgentStartPort = 4001,
[int] $CoordinatorStartPort = 8530,
[int] $DBServerStartPort = 8629
)

$ports=@()
Remove-Item -Recurse -Force cluster | Out-Null
New-Item -Path cluster -Force -ItemType Directory | Out-Null

$processes = @()
for ($i=0;$i -lt $AgentCount;$i++) {
    $port = $AgentStartPort + $i
    $ports+=$port
    Write-Host "Starting AGENT on $port"
    $processes += Start-Process `
        -RedirectStandardError "cluster\$port.err" `
        -RedirectStandardOutput "cluster\$port.out" `
        -FilePath .\build\bin\arangod.exe `
        -ArgumentList @(
            "-c", "none",
            "--agency.activate=true",
            "--agency.size=$AgentCount",
            "--agency.my-address=tcp://[::1]:$port",
            "--agency.endpoint=tcp://[::1]:$AgentStartPort",
            "--database.directory=cluster\data$port",
            "--javascript.app-path=.\js\apps",
            "--javascript.startup-directory=.\js",
            "--javascript.module-directory=.\enterprise\js",
            "--javascript.v8-contexts=1",
            "--server.endpoint=tcp://[::1]:$port",
            "--server.authentication=false",
            "--server.statistics=false",
            "--log.file=cluster\$port.log",
            "--log.force-direct=true"
        )`
        -NoNewWindow `
        -PassThru
}

$map=@{
    "COORDINATOR"= @{
        "count"= $CoordinatorCount;
        "startport"= $CoordinatorStartPort
    }
    "PRIMARY"= @{
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
        Write-Host "Starting $role on $port"
        $arguments = @(
            "-c", "none",
            "--cluster.agency-endpoint=tcp://[::1]:$AgentStartPort",
            "--cluster.my-role=$role",
            "--cluster.my-address=tcp://[::1]:$port",
            "--database.directory=cluster\data$port",
            "--javascript.app-path=.\js\apps",
            "--javascript.startup-directory=.\js",
            "--javascript.module-directory=.\enterprise\js",
            "--javascript.v8-contexts=1",
            "--server.endpoint=tcp://[::1]:$port",
            "--server.authentication=false",
            "--log.file=cluster\$port.log",
            "--log.force-direct=true"
        )
        $processes += Start-Process `
            -RedirectStandardError "cluster\$port.err" `
            -RedirectStandardOutput "cluster\$port.out" `
            -FilePath .\build\bin\arangod.exe `
            -ArgumentList $arguments `
            -NoNewWindow `
            -PassThru
    }
}

while ($ports -gt 0) {
    $newPorts=@()
    foreach ($port in $ports) {
        try {
            ($result=Invoke-WebRequest -TimeoutSec 1 -UseBasicParsing -URI "http://localhost:$port/_api/version" 2>&1)  | Out-Null
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
Write-Host ""