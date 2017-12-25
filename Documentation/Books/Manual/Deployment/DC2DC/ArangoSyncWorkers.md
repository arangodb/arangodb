# ArangoSync Workers

The _ArangoSync Worker_ is responsible for executing synchronization tasks.
<br/> For optimal performance at least 1 _worker_ instance must be placed on 
every machine that has an ArangoDB _DBserver_ running. This ensures that tasks 
can be executed with minimal network traffic outside of the machine.

Since _sync workers_ will automatically stop once their TLS server certificate expires 
(which is set to 2 years by default), it is recommended to run at least 2 instances
of a _worker_ on every machine in the datacenter. That way, tasks can still be 
assigned in the most optimal way, even when a _worker_ in temporarily down for a
restart.

To start an _ArangoSync Worker_ using a `systemd` service, use a unit like this:

```
[Unit]
Description=Run ArangoSync in worker mode 
After=network.target

[Service]
Restart=on-failure
EnvironmentFile=/etc/arangodb.env 
EnvironmentFile=/etc/arangodb.env.local
Environment=PORT=8729
ExecStart=/usr/sbin/arangosync run worker \
    --log.level=debug \
    --server.port=${PORT} \
    --server.endpoint=https://${PRIVATEIP}:${PORT} \
    --master.endpoint=${MASTERENDPOINTS} \
    --master.jwtSecret=${MASTERSECRET}
TimeoutStopSec=60

[Install]
WantedBy=multi-user.target
```

The _ArangoSync Worker_ must be reachable on a TCP port `${PORT}` (used with `--server.port`
option). This port must be reachable from inside the datacenter (by _sync masters_).

