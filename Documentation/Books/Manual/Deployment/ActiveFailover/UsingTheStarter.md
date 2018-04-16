Using the ArangoDB Starter
==========================

If you want to start a resilient single database server, use `--starter.mode=resilientsingle`.
In this mode a 3 machine _Agency_ is started as well as 2 single servers that perform
asynchronous replication an failover, if needed:

```bash
arangodb --starter.mode=resilientsingle --starter.join A,B,C
```

Run this on machine A, B & C.

The _Starter_ will decide on which 2 machines to run a single server instance.
To override this decision (only valid while bootstrapping), add a
`--cluster.start-single=false` to the machine where the single server
instance should NOT be scheduled.

Starting a resilient single server pair in Docker
-------------------------------------------------

If you want to start a resilient single database server running in docker containers,
use the normal docker arguments, combined with `--starter.mode=resilientsingle`.

```bash
export IP=<IP of docker host>
docker volume create arangodb
docker run -it --name=adb --rm -p 8528:8528 \
    -v arangodb:/data \
    -v /var/run/docker.sock:/var/run/docker.sock \
    arangodb/arangodb-starter \
    --starter.address=$IP \
    --starter.mode=resilientsingle \
    --starter.join=A,B,C
```

Run this on machine A, B & C.

The starter will decide on which 2 machines to run a single server instance.
To override this decision (only valid while bootstrapping), add a
`--cluster.start-single=false` to the machine where the single server
instance should NOT be scheduled.

Starting a local test resilient single sever pair
-------------------------------------------------

If you want to start a local resilient server pair quickly, use the `--starter.local` flag.
It will start all servers within the context of a single starter process.

```bash
arangodb --starter.local --starter.mode=resilientsingle
```

**Note:** When you restart the started, it remembers the original `--starter.local` flag.