<!-- don't edit here, it's from https://@github.com/arangodb/arangosync.git / docs/Manual/ -->
# ArangoSync Workers

The _ArangoSync Worker_ is responsible for executing synchronization tasks.

For optimal performance at least 1 _worker_ instance must be placed on
every machine that has an ArangoDB _DBserver_ running. This ensures that tasks
can be executed with minimal network traffic outside of the machine.

Since _sync workers_ will automatically stop once their TLS server certificate expires
(which is set to 2 years by default), it is recommended to run at least 2 instances
of a _worker_ on every machine in the datacenter. That way, tasks can still be
assigned in the most optimal way, even when a _worker_ is temporarily down for a
restart.

To start an _ArangoSync Worker_ using a `systemd` service, use a unit like this:

```text
[Unit]
Description=Run ArangoSync in worker mode
After=network.target

[Service]
Restart=on-failure
EnvironmentFile=/etc/arangodb.env
EnvironmentFile=/etc/arangodb.env.local
Environment=PORT=8729
LimitNOFILE=1000000
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

Note the large file descriptor limit. The _sync worker_ requires about 30 file descriptors per
shard. If you use hardware with huge resources, and still run out of file descriptors,
you can decide to run multiple _sync workers_ on each machine in order to spread the tasks across them.

## Recommended deployment environment

The _sync workers_ should be run on all machines that also contain an ArangoDB _DBServer_.
The _sync worker_ can be memory intensive when running lots of databases & collections.

Consider these machines "cattle".
