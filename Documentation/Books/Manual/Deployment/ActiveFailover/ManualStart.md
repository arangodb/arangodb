Starting Manually
=================

An ArangoDB _Active Failover_ setup consists of several running _tasks_ or _processes_. 

This section describes how to start an _Active Failover_ by manually starting all
the needed processes.

Before continuing, be sure to read the [Architecture](../../Architecture/DeploymentModes/ActiveFailover/Architecture.md)
section to get a basic understanding of the underlying architecture and the involved
roles in an ArangoDB  Active Failover setup.

We will include commands for a local test (all processes running on a single machine)
and for a more real production scenario, which makes use of 3 different machines.

Local Tests
-----------

In this paragraph we will include commands to manually start an Active Failover
with 3 _Agents_, and two single server instances.

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

### Single Server Test Instances

To start the two single server instances, you can use the following commands:

```
arangod --server.authentication false \
	--server.endpoint tcp://127.0.0.1:6001 \
	--cluster.my-address tcp://127.0.0.1:6001 \
	--cluster.my-role SINGLE \
	--cluster.agency-endpoint tcp://127.0.0.1:5001 \
	--cluster.agency-endpoint tcp://127.0.0.1:5002 \
	--cluster.agency-endpoint tcp://127.0.0.1:5003 \ 
	--replication.automatic-failover true \
	--database.directory singleserver6001 &
 
arangod --server.authentication false \
	--server.endpoint tcp://127.0.0.1:6002 \
	--cluster.my-address tcp://127.0.0.1:6002 \
	--cluster.my-role SINGLE \
	--cluster.agency-endpoint tcp://127.0.0.1:5001 \
	--cluster.agency-endpoint tcp://127.0.0.1:5002 \
	--cluster.agency-endpoint tcp://127.0.0.1:5003 \
	--replication.automatic-failover true \
	--database.directory singleserver6002 &
```	

Multiple Machines
-----------------

The method from the previous paragraph can be extended to a more real production scenario,
to start an Active Failover on multiple machines. The only changes are that one
has to replace all local addresses `127.0.0.1` by the actual IP address of the
corresponding server. Obviously, it would no longer be necessary to use different
port numbers on different servers.

Let's assume that you want to start you Active Failover with 3 _Agents_ and two
single servers on three different machines with IP addresses:

```
192.168.1.1
192.168.1.2
192.168.1.3
```

Let's also suppose that each of the above machines runs an _Agent_, an the first
and second machine run also the single instance.

If we use:

- _8531_ as port of the _Agents_
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

### Single Server Instances

On 192.168.1.1:

```
arangod --server.authentication=false \
	--server.endpoint tcp://0.0.0.0:8529 \
	--cluster.my-address tcp://192.168.1.1:8529 \
	--cluster.my-role SINGLE \
	--cluster.agency-endpoint tcp://192.168.1.1:8531 \
	--cluster.agency-endpoint tcp://192.168.1.2:8531 \
	--cluster.agency-endpoint tcp://192.168.1.3:8531 \
	--replication.automatic-failover true \
	--database.directory singleserver &
```

On 192.168.1.2:

Wait until the previous server is fully started, then start the second single server
instance:

```
arangod --server.authentication=false \
	--server.endpoint tcp://0.0.0.0:8529 \
	--cluster.my-address tcp://192.168.1.2:8529 \
	--cluster.my-role SINGLE \
	--cluster.agency-endpoint tcp://192.168.1.1:8531 \
	--cluster.agency-endpoint tcp://192.168.1.2:8531 \
	--cluster.agency-endpoint tcp://192.168.1.3:8531 \
	--replication.automatic-failover true \
	--database.directory singleserver &
```

**Note:** in the above commands, you can use host names, if they can be resolved,
instead of IP addresses.

Manual Start in Docker
----------------------

Manually starting an _Active Failover_ via Docker is basically the same as described in the 
paragraphs above. 

A bit of extra care has to be invested due to the way in which Docker isolates its network. 
By default it fully isolates the network and by doing so an endpoint like `--server.endpoint tcp://0.0.0.0:8529`
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
docker run -e ARANGO_NO_AUTH=1 -p 192.168.1.1:10000:8529 arangodb/arangodb arangod \
	--server.endpoint tcp://0.0.0.0:8529\
	--cluster.my-address tcp://192.168.1.1:10000 \
	--cluster.my-role SINGLE \
	--cluster.agency-endpoint tcp://192.168.1.1:9001 \
	--cluster.agency-endpoint tcp://192.168.1.2:9001 \
	--cluster.agency-endpoint tcp://192.168.1.3:9001 \
	--replication.automatic-failover true 
```

This will start a single server within a Docker container with an isolated network. 
Within the Docker container it will bind to all interfaces (this will be 127.0.0.1:8529
and some internal Docker IP on port 8529). By supplying `-p 192.168.1.1:10000:8529`
we are establishing a port forwarding from our local IP (192.168.1.1 port 10000 in
this example) to port 8529 inside the container. Within the command we are telling
_arangod_ how it can be reached from the outside `--cluster.my-address tcp://192.168.1.1:10000`.

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
