Manual Start
============

An ArangoDB Cluster consists of several running tasks (or server processeses) which
form the Cluster. 

This section describes how to start a Cluster by manually starting all the needed
processes. The required parameters for every role in an ArangoDB Cluster are discussed.

Before continuing, be sure to read the [Architecture](../Scalability/Cluster/Architecture.md)
section to get a basic understanding of the underlying architecture and the involved
roles in an ArangoDB Cluster.

We will include commands for a local test (all processes running on a single machine)
and for a more real production scenario, which makes use of 3 different machines.

Local Test: all processes in a single machine (testing only) 
------------------------------------------------------------

In this section we will include commands to manually start a Cluster with 3 _Agents_,
2 _DBservers_ and 2 _Coordinators_

We will assume that all processes runs on the same machine (127.0.0.1).

### Agency

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

### DBServers and Coordinators

These two roles share a common set of relevant options. First you should specify
the role using `--cluster.my-role`. This can either be `PRIMARY` (a database server)
or `COORDINATOR`. Furthermore provide the external endpoint (IP and port) of the
process via `--cluster.my-address`.

The following is a full-example of what it might look like.

**DBServers:**

```
arangod --server.authentication=false \
	--server.endpoint tcp://0.0.0.0:6001 \
	--cluster.my-address tcp://127.0.0.1:6001 \
	--cluster.my-role PRIMARY \
	--cluster.agency-endpoint tcp://127.0.0.1:5001 \
	--cluster.agency-endpoint tcp://127.0.0.1:5002 \
	--cluster.agency-endpoint tcp://127.0.0.1:5003 \
	--database.directory dbserver1 &

arangod --server.authentication=false \
	--server.endpoint tcp://0.0.0.0:6002 \
	--cluster.my-address tcp://127.0.0.1:6002 \
	--cluster.my-role PRIMARY \
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

The method from the previous section can be extended to a more real production scenario,
to start an ArangoDB Cluster on multiple machines. The only changes are that one
has to replace all local addresses `127.0.0.1` by the actual IP address of the
corresponding server. Obviously, it would no longer be necessary to use different port numbers
on different servers.

Let's assume that you want to start you ArangoDB Cluster with 3 _Agents_, 3 _DBServers_
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

then the commands you have to use are reported in the following sections.

### Agency  
 
On 192.168.1.1:

```
sudo arangod --server.endpoint tcp://0.0.0.0:8531 \
	--agency.my-address tcp://192.168.1.1:8531 \
	--server.authentication false \
	--agency.activate true \
	--agency.size 3 \
	--agency.supervision true \
	--database.directory agent 
```

On 192.168.1.2:

```
sudo arangod --server.endpoint tcp://0.0.0.0:8531 \
	--agency.my-address tcp://192.168.1.2:8531 \
	--server.authentication false \
	--agency.activate true \
	--agency.size 3 \
	--agency.supervision true \
	--database.directory agent
```

On 192.168.1.3:

```
sudo arangod --server.endpoint tcp://0.0.0.0:8531 \
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

On 192.168.1.1:

```
sudo arangod --server.authentication=false \
	--server.endpoint tcp://0.0.0.0:8530 \
	--cluster.my-address tcp://192.168.1.1:8530 \
	--cluster.my-role PRIMARY \
	--cluster.agency-endpoint tcp://192.168.1.1:8531 \
	--cluster.agency-endpoint tcp://192.168.1.2:8531 \
	--cluster.agency-endpoint tcp://192.168.1.3:8531 \
	--database.directory dbserver &
```

On 192.168.1.2:

```
sudo arangod --server.authentication=false \
	--server.endpoint tcp://0.0.0.0:8530 \
	--cluster.my-address tcp://192.168.1.2:8530 \
	--cluster.my-role PRIMARY \
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
	--cluster.my-role PRIMARY \
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
sudo arangod --server.authentication=false \
	--server.endpoint tcp://0.0.0.0:8530 \
	--cluster.my-address tcp://192.168.4.1:8530 \
	--cluster.my-role PRIMARY \
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
