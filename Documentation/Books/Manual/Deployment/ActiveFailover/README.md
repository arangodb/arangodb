# Active Failover deployment

This _Section_ describes how to deploy an _Active Failover_ environement.
For a general introduction to _Active Failover_, please refer to the [Active Failover](../../Scalability/ActiveFailiver/README.md) chapter.

As usual there are two main ways to start an Active-Failover setup:
Either [manually](Starting Manually) or using the [ArangoDB starter](Using the ArangoDB Starter) 
(possibly in conjunction with docker).

## Starting Manually

We are going to start two single server instances and one agency.
First we need to start the 
```bash
    arangod \
        --agency.activate true \
        --agency.endpoint tcp://agency.domain.org:4001 \
        --agency.my-address tcp://agency.domain.org:4001 \
        --agency.pool-size 1 \
        --agency.size 1 \
        --database.directory dbdir/data4001 \
        --javascript.v8-contexts 1 \
        --server.endpoint tcp://agency.domain.org:4001 \
        --server.statistics false \
        --server.threads 16 \
        --log.file dbdir/4001.log \
        --log.level INFO \
        | tee dbdir/4001.stdout 2>&1 &
```

Next we are going to start the leader (wait until this server is fully started)
```bash
    arangod \
      --database.directory dbdir/data8530 \
      --cluster.agency-endpoint tcp://agency.domain.org:4001 \
      --cluster.my-address tcp://leader.domain.org:4001 \
      --server.endpoint tcp://leader.domain.org:4001 \
      --cluster.my-role SINGLE \
      --replication.active-failover true \
      --log.file dbdir/8530.log \
      --server.statistics true \
      --server.threads 5 \
      | tee cluster/8530.stdout 2>&1 &
```

After the Leader server is fully started then you can add additional Followers,
with basically the same startup parameters (except for their address and database directory)

```bash
    arangod \
      --database.directory dbdir/data8531 \
      --cluster.agency-endpoint tcp://agency.domain.org:4001 \
      --cluster.my-address tcp://leader.domain.org:4001 \
      --server.endpoint tcp://leader.domain.org:4001 \
      --cluster.my-role SINGLE \
      --replication.active-failover true \
      --log.file dbdir/8531.log \
      --server.statistics true \
      --server.threads 5 \
      | tee cluster/8531.stdout 2>&1 &
```


## Using the ArangoDB Starter

If you want to start a resilient single database server, use `--starter.mode=resilientsingle`.
In this mode a 3 machine agency is started and 2 single servers that perform
asynchronous replication an failover if needed.

```bash
arangodb --starter.mode=resilientsingle --starter.join A,B,C
```

Run this on machine A, B & C.

The starter will decide on which 2 machines to run a single server instance.
To override this decision (only valid while bootstrapping), add a
`--cluster.start-single=false` to the machine where the single server
instance should NOT be scheduled.

### Starting a resilient single server pair in Docker

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

### Starting a local test resilient single sever pair

If you want to start a local resilient server pair quickly, use the `--starter.local` flag.
It will start all servers within the context of a single starter process.

```bash
arangodb --starter.local --starter.mode=resilientsingle
```

Note: When you restart the started, it remembers the original `--starter.local` flag.
