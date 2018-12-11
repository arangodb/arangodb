<!-- don't edit here, it's from https://@github.com/arangodb-helper/arangodb.git / docs/Manual/ -->
# Starting an ArangoDB cluster or database the easy way

Starting an ArangoDB cluster involves starting various servers with
different roles (agents, dbservers & coordinators).

The ArangoDB Starter is designed to make it easy to start and
maintain an ArangoDB cluster or single server database.

Besides starting and maintaining ArangoDB deployments, the starter also provides
various commands to create TLS certificates & JWT token secrets to secure your
ArangoDB deployment.

## Installation

The ArangoDB starter (`arangodb`) comes with all current distributions of ArangoDB.

If you want a specific version, download the precompiled binary via the
[GitHub releases page](https://github.com/arangodb-helper/arangodb/releases).

## Starting a cluster

An ArangoDB cluster typically involves 3 machines.
ArangoDB must be installed on all of them.

Then start the ArangoDB starter of all 3 machines like this:

On host A:

```bash
arangodb
```

This will use port 8528 to wait for colleagues (3 are needed for a
resilient agency). On host B (can be the same as A):

```bash
arangodb --starter.join A
```

This will contact A on port 8528 and register. On host C (can be same
as A or B):

```bash
arangodb --starter.join A
```

This will contact A on port 8528 and register.

From the moment on when 3 have joined, each will fire up an agent, a
coordinator and a dbserver and the cluster is up. Ports are shown on
the console, the starter uses the next few ports above the starter
port. That is, if one uses port 8528 for the starter, the coordinator
will use 8529 (=8528+1), the dbserver 8530 (=8528+2), and the agent 8531
(=8528+3). You can change the default starter port with the `--starter.port`
[option](../../Programs/Starter/Options.md).

Additional servers can be added in the same way.

If two or more of the `arangodb` instances run on the same machine,
one has to use the `--starter.data-dir` option to let each use a different
directory.

The `arangodb` program will find the ArangoDB executable (`arangod`) and the
other installation files automatically. If this fails, use the
`--server.arangod` and `--server.js-dir` options described below.

## Running in Docker

You can run `arangodb` using our ready made docker container.

When using `arangodb` in a Docker container it will also run all
servers in a docker using the `arangodb/arangodb:latest` docker image.
If you wish to run a specific docker image for the servers, specify it using
the `--docker.image` argument.

When running in docker it is important to care about the volume mappings on
the container. Typically you will start the executable in docker with the following
commands.

```bash
export IP=<IP of docker host>
docker volume create arangodb1
docker run -it --name=adb1 --rm -p 8528:8528 \
    -v arangodb1:/data \
    -v /var/run/docker.sock:/var/run/docker.sock \
    arangodb/arangodb-starter \
    --starter.address=$IP
```

The executable will show the commands needed to run the other instances.

Note that the commands above create a docker volume. If you're running on Linux
it is also possible to use a host mapped volume. Make sure to map it on `/data`.

## Using multiple join arguments

It is allowed to use multiple `--starter.join` arguments.
This eases scripting. For example:

On host A:

```bash
arangodb --starter.join A,B,C
```

On host B:

```bash
arangodb --starter.join A,B,C
```

On host C:

```bash
arangodb --starter.join A,B,C
```

This starts a cluster where the starter on host A is chosen to be master during the bootstrap phase.

Note: `arangodb --starter.join A,B,C` is equal to `arangodb --starter.join A --starter.join B --starter.join C`.

During the bootstrap phase of the cluster, the starters will all choose the "master" starter
based on list of given `starter.join` arguments.

The "master" starter is chosen as follows:

- If there are no `starter.join` arguments, the starter becomes a master.
- If there are multiple `starter.join` arguments, these arguments are sorted. If a starter is the first
  in this sorted list, it becomes a starter.
- In all other cases, the starter becomes a slave.

Note: Once the bootstrap phase is over (all arangod servers have started and are running), the bootstrap
phase ends and the starters use the Arango agency to elect a master for the runtime phase.

## Starting a local test cluster

If you want to start a local cluster quickly, use the `--starter.local` flag.
It will start all servers within the context of a single starter process.

```bash
arangodb --starter.local
```

Using the starter this way does not provide resilience and high availability of your cluster!

Note: When you restart the starter, it remembers the original `--starter.local` flag.

## Starting a cluster with datacenter to datacenter synchronization

{% hint 'info' %}
This feature is only available in the
[**Enterprise Edition**](https://www.arangodb.com/why-arangodb/arangodb-enterprise/)
{% endhint %}

Datacenter to datacenter replication (DC2DC) requires a normal ArangoDB cluster in both data centers
and one or more (`arangosync`) syncmasters & syncworkers in both data centers.
The starter enables you to run these syncmasters & syncworkers in combination with your normal
cluster.

To run a starter with DC2DC support you add the following arguments to the starters command line:

```bash
--auth.jwt-secret=<path of file containing JWT secret for communication in local cluster>
--starter.address=<publicly visible address of this machine>
--starter.sync
--server.storage-engine=rocksdb
--sync.master.jwt-secret=<path of file containing JWT secret used for communication between local syncmaster & workers>
--sync.server.keyfile=<path of keyfile containing TLS certificate & key for local syncmaster>
--sync.server.client-cafile=<path of file containing CA certificate for syncmaster client authentication>
```

Consult `arangosync` documentation for instructions how to create all certificates & keyfiles.

## Starting a single server

If you want to start a single database server, use `--starter.mode=single`.

```bash
arangodb --starter.mode=single
```

## Starting a single server in Docker

If you want to start a single database server running in a docker container,
use the normal docker arguments, combined with `--starter.mode=single`.

```bash
export IP=<IP of docker host>
docker volume create arangodb
docker run -it --name=adb --rm -p 8528:8528 \
    -v arangodb:/data \
    -v /var/run/docker.sock:/var/run/docker.sock \
    arangodb/arangodb-starter \
    --starter.address=$IP \
    --starter.mode=single
```

## Starting a resilient single server pair

If you want to start a resilient single database server, use `--starter.mode=activefailover`.
In this mode a 3 machine agency is started and 2 single servers that perform
asynchronous replication an failover if needed.

```bash
arangodb --starter.mode=activefailover --starter.join A,B,C
```

Run this on machine A, B & C.

The starter will decide on which 2 machines to run a single server instance.
To override this decision (only valid while bootstrapping), add a
`--cluster.start-single=false` to the machine where the single server
instance should NOT be scheduled.

## Starting a resilient single server pair in Docker

If you want to start a resilient single database server running in docker containers,
use the normal docker arguments, combined with `--starter.mode=activefailover`.

```bash
export IP=<IP of docker host>
docker volume create arangodb
docker run -it --name=adb --rm -p 8528:8528 \
    -v arangodb:/data \
    -v /var/run/docker.sock:/var/run/docker.sock \
    arangodb/arangodb-starter \
    --starter.address=$IP \
    --starter.mode=activefailover \
    --starter.join=A,B,C
```

Run this on machine A, B & C.

The starter will decide on which 2 machines to run a single server instance.
To override this decision (only valid while bootstrapping), add a
`--cluster.start-single=false` to the machine where the single server
instance should NOT be scheduled.

## Starting a local test resilient single sever pair

If you want to start a local resilient server pair quickly, use the `--starter.local` flag.
It will start all servers within the context of a single starter process.

```bash
arangodb --starter.local --starter.mode=activefailover
```

Note: When you restart the started, it remembers the original `--starter.local` flag.

## Starting & stopping in detached mode

If you want the starter to detach and run as a background process, use the `start`
command. This is typically used by developers running tests only.

```bash
arangodb start --starter.local=true [--starter.wait]
```

This command will make the starter run another starter process in the background
(that starts all ArangoDB servers), wait for it's HTTP API to be available and
then exit. The starter that was started in the background will keep running until you stop it.

The `--starter.wait` option makes the `start` command wait until all ArangoDB server
are really up, before ending the master process.

To stop a starter use this command.

```bash
arangodb stop
```

Make sure to match the arguments given to start the starter (`--starter.port` & `--ssl.*`).

## More information

- [Options](../../Programs/Starter/Options.md) contains a list of all commandline options supported by the starter.
- [Security](../../Programs/Starter/Security.md) contains instructions of how to create certificates & tokens needed
  to secure an ArangoDB deployment.
