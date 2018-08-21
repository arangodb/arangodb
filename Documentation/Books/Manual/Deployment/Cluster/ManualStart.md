Starting Manually
=================

An ArangoDB Cluster consists of several running _tasks_ or _processes_ which
form the Cluster. 

This section describes how to start a Cluster by manually starting all the needed
processes.

Before continuing, be sure to read the [Architecture](../../Architecture/DeploymentModes/Cluster/Architecture.md)
section to get a basic understanding of the underlying architecture and the involved
roles in an ArangoDB Cluster.

We will include commands for a local test (all processes running on a single machine)
and for a more real production scenario, which makes use of 3 different machines.

Local Tests
-----------

In this paragraph we will include commands to manually start a Cluster with 3 _Agents_,
2 _DBservers_ and 2 _Coordinators_.

We will assume that all processes runs on the same machine (127.0.0.1). Such scenario
should be used for testing only.

### Local Test Agency

To start up an _Agency_ you first have to activate it. This is done by providing
the option `--agency.activate true`.

To start up the _Agency_ in its fault tolerant mode set the `--agency.size` to `3`.
You will then have to start at least 3 _Agents_ before the _Agency_ will start operation.

During initialization the _Agents_ have to find each other. To do so provide at
least one common `--agency.endpoint`. The _Agents_ will then coordinate startup
themselves. They will announce themselves with their external address which may be
specified using `--agency.my-address`. This is required in bridged docker setups
or NATed environments.

So in summary these are the commands to start an _Agency_ of size 3:

```
arangod --server.endpoint tcp://0.0.0.0:5001 \
	--agency.my-address=tcp://127.0.0.1:5001 \
	--server.authentication false \
	--agency.activate true \
	--agency.size 3 \
	--agency.endpoint tcp://127.0.0.1:5001 \
	--agency.supervision true \
	--database.directory agent1 &
   
arangod --server.endpoint tcp://0.0.0.0:5002 \
	--agency.my-address=tcp://127.0.0.1:5002 \
	--server.authentication false \
	--agency.activate true \
	--agency.size 3 \
	--agency.endpoint tcp://127.0.0.1:5001 \
	--agency.supervision true \
	--database.directory agent2 &

arangod --server.endpoint tcp://0.0.0.0:5003 \
	--agency.my-address=tcp://127.0.0.1:5003 \
	--server.authentication false \
	--agency.activate true \
	--agency.size 3 \
	--agency.endpoint tcp://127.0.0.1:5001 \
	--agency.supervision true \
	--database.directory agent3 &
```

### Local Test DBServers and Coordinators

These two roles share a common set of relevant options. First you should specify
the role using `--cluster.my-role`. This can either be `PRIMARY` (a database server)
or `COORDINATOR`. Note that starting from v.3.4 `DBSERVER` is allowed as an alias
for `PRIMARY` as well. Furthermore please provide the external endpoint (IP and port)
of the process via `--cluster.my-address`.

The following is a full example of what it might look like.

**DBServers:**

```
arangod --server.authentication=false \
	--server.endpoint tcp://0.0.0.0:6001 \
	--cluster.my-address tcp://127.0.0.1:6001 \
	--cluster.my-role DBSERVER \
	--cluster.agency-endpoint tcp://127.0.0.1:5001 \
	--cluster.agency-endpoint tcp://127.0.0.1:5002 \
	--cluster.agency-endpoint tcp://127.0.0.1:5003 \
	--database.directory dbserver1 &

arangod --server.authentication=false \
	--server.endpoint tcp://0.0.0.0:6002 \
	--cluster.my-address tcp://127.0.0.1:6002 \
	--cluster.my-role DBSERVER \
	--cluster.agency-endpoint tcp://127.0.0.1:5001 \
	--cluster.agency-endpoint tcp://127.0.0.1:5002 \
	--cluster.agency-endpoint tcp://127.0.0.1:5003 \
	--database.directory dbserver2 &
```

**Coordinators:**

```
arangod --server.authentication=false \
	--server.endpoint tcp://0.0.0.0:7001 \
	--cluster.my-address tcp://127.0.0.1:7001 \
	--cluster.my-role COORDINATOR \
	--cluster.agency-endpoint tcp://127.0.0.1:5001 \
	--cluster.agency-endpoint tcp://127.0.0.1:5002 \
	--cluster.agency-endpoint tcp://127.0.0.1:5003 \
	--database.directory coordinator1 &
```

```
arangod --server.authentication=false \
	--server.endpoint tcp://0.0.0.0:7002 \
	--cluster.my-address tcp://127.0.0.1:7002 \
	--cluster.my-role COORDINATOR \
	--cluster.agency-endpoint tcp://127.0.0.1:5001 \
	--cluster.agency-endpoint tcp://127.0.0.1:5002 \
	--cluster.agency-endpoint tcp://127.0.0.1:5003 \
	--database.directory coordinator2 &
```

Note in particular that the endpoint descriptions given under `--cluster.my-address`
and `--cluster.agency-endpoint` must not use the IP address `0.0.0.0` because they
must contain an actual address that can be routed to the corresponding server. The
`0.0.0.0` in `--server.endpoint` simply means that the server binds itself to all
available network devices with all available IP addresses.

Upon registering with the _Agency_ during startup the Cluster will assign an _ID_
to every server. The generated _ID_ will be printed out to the log or can be accessed
via the HTTP API by calling `http://server-address/_admin/server/id`.

You have now launched an ArangoDB Cluster and can contact its _Coordinators_ (and
their corresponding web UI) at the endpoint `tcp://127.0.0.1:7001` and `tcp://127.0.0.1:7002`.

Multiple Machines
-----------------

The method from the previous paragraph can be extended to a more real production scenario,
to start an ArangoDB Cluster on multiple machines. The only changes are that one
has to replace all local addresses `127.0.0.1` by the actual IP address of the
corresponding server. Obviously, it would no longer be necessary to use different port numbers
on different servers.

Let's assume that you want to start your ArangoDB Cluster with 3 _Agents_, 3 _DBServers_
and 3 _Coordinators_ on three different machines with IP addresses:

```
192.168.1.1
192.168.1.2
192.168.1.3
```

Let's also suppose that each of the above machines runs an _Agent_, a _DBServer_
and a _Coordinator_

If we use:

- _8531_ as port of the _Agents_
- _8530_ as port of the _DBServers_
- _8529_ as port of the _Coordinators_

then the commands you have to use are reported in the following subparagraphs.

### Agency
 
On 192.168.1.1:

```
arangod --server.endpoint tcp://0.0.0.0:8531 \
	--agency.my-address tcp://192.168.1.1:8531 \
	--server.authentication false \
	--agency.activate true \
	--agency.size 3 \
	--agency.supervision true \
	--database.directory agent 
```

On 192.168.1.2:

```
arangod --server.endpoint tcp://0.0.0.0:8531 \
	--agency.my-address tcp://192.168.1.2:8531 \
	--server.authentication false \
	--agency.activate true \
	--agency.size 3 \
	--agency.supervision true \
	--database.directory agent
```

On 192.168.1.3:

```
arangod --server.endpoint tcp://0.0.0.0:8531 \
	--agency.my-address tcp://192.168.1.3:8531 \
	--server.authentication false \
	--agency.activate true \
	--agency.size 3 \
	--agency.endpoint tcp://192.168.1.1:8531 \
	--agency.endpoint tcp://192.168.1.2:8531 \ 
	--agency.endpoint tcp://192.168.1.3:8531 \
	--agency.supervision true \
	--database.directory agent
```

### DBServers

In the commands below, note that `DBSERVER`, as value of the option
`--cluster.my-role`, is allowed only from version 3.4; for previous
versions, to start a _DBServer_, please use `PRIMARY` as role.

On 192.168.1.1:

```
arangod --server.authentication=false \
	--server.endpoint tcp://0.0.0.0:8530 \
	--cluster.my-address tcp://192.168.1.1:8530 \
	--cluster.my-role DBSERVER \
	--cluster.agency-endpoint tcp://192.168.1.1:8531 \
	--cluster.agency-endpoint tcp://192.168.1.2:8531 \
	--cluster.agency-endpoint tcp://192.168.1.3:8531 \
	--database.directory dbserver &
```

On 192.168.1.2:

```
arangod --server.authentication=false \
	--server.endpoint tcp://0.0.0.0:8530 \
	--cluster.my-address tcp://192.168.1.2:8530 \
	--cluster.my-role DBSERVER \
	--cluster.agency-endpoint tcp://192.168.1.1:8531 \
	--cluster.agency-endpoint tcp://192.168.1.2:8531 \
	--cluster.agency-endpoint tcp://192.168.1.3:8531 \
	--database.directory dbserver &
```

On 192.168.1.3:

```
sudo arangod --server.authentication=false \
	--server.endpoint tcp://0.0.0.0:8530 \
	--cluster.my-address tcp://192.168.1.3:8530 \
	--cluster.my-role DBSERVER \
	--cluster.agency-endpoint tcp://192.168.1.1:8531 \
	--cluster.agency-endpoint tcp://192.168.1.2:8531 \
	--cluster.agency-endpoint tcp://192.168.1.3:8531 \
	--database.directory dbserver &
```

### Coordinators

On 192.168.1.1:

```
arangod --server.authentication=false \
	--server.endpoint tcp://0.0.0.0:8529 \
	--cluster.my-address tcp://192.168.1.1:8529 \
	--cluster.my-role COORDINATOR \
	--cluster.agency-endpoint tcp://192.168.1.1:8531 \
	--cluster.agency-endpoint tcp://192.168.1.2:8531 \
	--cluster.agency-endpoint tcp://192.168.1.3:8531 \
	--database.directory coordinator &
```

On 192.168.1.2:

```
arangod --server.authentication=false \
	--server.endpoint tcp://0.0.0.0:8529 \
	--cluster.my-address tcp://192.168.1.2:8529 \
	--cluster.my-role COORDINATOR \
	--cluster.agency-endpoint tcp://192.168.1.1:8531 \
	--cluster.agency-endpoint tcp://192.168.1.2:8531 \
	--cluster.agency-endpoint tcp://192.168.1.3:8531 \
	--database.directory coordinator &
```

On 192.168.1.3:

```
arangod --server.authentication=false \
	--server.endpoint tcp://0.0.0.0:8529 \
	--cluster.my-address tcp://192.168.1.3:8529 \
	--cluster.my-role COORDINATOR \
	--cluster.agency-endpoint tcp://192.168.1.1:8531 \
	--cluster.agency-endpoint tcp://192.168.1.2:8531 \
	--cluster.agency-endpoint tcp://192.168.1.3:8531 \
	--database.directory coordinator &
```

**Note:** in the above commands, you can use host names, if they can be resolved,
instead of IP addresses.

**Note 2:** you can easily extend the Cluster, by adding more machines which run
a _DBServer_ and a _Coordiantor_. For instance, if you have an additional forth
machine with IP 192.168.1.4, you can execute the following commands

On 192.168.1.4:

```
arangod --server.authentication=false \
	--server.endpoint tcp://0.0.0.0:8530 \
	--cluster.my-address tcp://192.168.4.1:8530 \
	--cluster.my-role DBSERVER \
	--cluster.agency-endpoint tcp://192.168.1.1:8531 \
	--cluster.agency-endpoint tcp://192.168.1.2:8531 \
	--cluster.agency-endpoint tcp://192.168.1.3:8531 \
	--database.directory dbserver &
	
arangod --server.authentication=false \
	--server.endpoint tcp://0.0.0.0:8529 \
	--cluster.my-address tcp://192.168.1.4:8529 \
	--cluster.my-role COORDINATOR \
	--cluster.agency-endpoint tcp://192.168.1.1:8531 \
	--cluster.agency-endpoint tcp://192.168.1.2:8531 \
	--cluster.agency-endpoint tcp://192.168.1.3:8531 \
	--database.directory coordinator &
```

Manual Start in Docker
----------------------

Manually starting a Cluster via Docker is basically the same as described in the 
paragraphs above. 

A bit of extra care has to be invested due to the way in which Docker isolates its network. 
By default it fully isolates the network and by doing so an endpoint like `--server.endpoint tcp://0.0.0.0:8530`
will only bind to all interfaces inside the Docker container which does not include
any external interface on the host machine. This may be sufficient if you just want
to access it locally but in case you want to expose it to the outside you must
facilitate Dockers port forwarding using the `-p` command line option. Be sure to
check the [official Docker documentation](https://docs.docker.com/engine/reference/run/).

You can simply use the `-p` flag in Docker to make the individual processes available on the host
machine or you could use Docker's [links](https://docs.docker.com/engine/reference/run/)
to enable process intercommunication.

An example configuration might look like this:

```
docker run -e ARANGO_NO_AUTH=1 -p 192.168.1.1:10000:8530 arangodb/arangodb arangod \
	--server.endpoint tcp://0.0.0.0:8530 \
	--cluster.my-address tcp://192.168.1.1:10000 \
	--cluster.my-role DBSERVER \
	--cluster.agency-endpoint tcp://192.168.1.1:9001 \
	--cluster.agency-endpoint tcp://192.168.1.2:9001 \
	--cluster.agency-endpoint tcp://192.168.1.3:9001 
```

This will start a _DBServer_ within a Docker container with an isolated network. 
Within the Docker container it will bind to all interfaces (this will be 127.0.0.1:8530
and some internal Docker IP on port 8530). By supplying `-p 192.168.1.1:10000:8530`
we are establishing a port forwarding from our local IP (192.168.1.1 port 10000 in
this example) to port 8530 inside the container. Within the command we are telling
_arangod_ how it can be reached from the outside `--cluster.my-address tcp://192.168.1.1:10000`.
This information will be forwarded to the _Agency_ so that the other processes in
your Cluster can see how this particular _DBServer_ may be reached.

### Authentication

To start the official Docker container you will have to decide on an authentication
method, otherwise the container will not start.

Provide one of the arguments to Docker as an environment variable. There are three
options:

1. ARANGO_NO_AUTH=1

   Disable authentication completely. Useful for local testing or for operating
   in a trusted network (without a public interface).
        
2. ARANGO_ROOT_PASSWORD=password

   Start ArangoDB with the given password for root.
        
3. ARANGO_RANDOM_ROOT_PASSWORD=1

   Let ArangoDB generate a random root password.
       
For an in depth guide about Docker and ArangoDB please check the official documentation:
https://hub.docker.com/r/arangodb/arangodb/ . Note that we are using the image
`arangodb/arangodb` here which is always the most current one. There is also the
"official" one called `arangodb` whose documentation is here: https://hub.docker.com/_/arangodb/
