Launching an ArangoDB cluster for testing
-----------------------------------------

An ArangoDB cluster consists of several running tasks which form the cluster. ArangoDB itself won't start or monitor any of these tasks. So it will need some kind of supervisor which is monitoring and starting these tasks. For production usage we recommend using Apache Mesos as the cluster supervisor.

However starting a cluster manually is possible and is a very easy method to get a first impression of what an ArangoDB cluster looks like.

The easiest way to start a local cluster for testing purposes is to run `scripts/startLocalCluster.sh` from a clone of the [source repository](https://github.com/ArangoDB/ArangoDB) after compiling ArangoDB from source (see instructions in the file `README_maintainers.md` in the repository. This will start 1 Agency, 2 DBServers and 1 Coordinator. To stop the cluster issue `scripts/stopLocalCluster.sh`.

This section will discuss the required parameters for every role in an ArangoDB cluster. Be sure to read the [Architecture](../Scalability/Architecture.md) documentation to get a basic understanding of the underlying architecture and the involved roles in an ArangoDB cluster.

In the following sections we will go through the relevant options per role.

![single node cluster](simple_cluster.png)

### Agency

To start up an agency you first have to activate it. This is done by providing `--agency.activate true`.

To start up the agency in its fault tolerant mode set the `--agency.size` to `3`. You will then have to provide at least 3 agents before the agency will start operation.

During initialization the agents have to find each other. To do so provide at least one common `--agency.endpoint`. The agents will then coordinate startup themselves. They will announce themselves with their external address which may be specified using `--agency.my-address`. This is required in bridged docker setups or NATed environments.

So in summary this is what your startup might look like:

```
arangod --server.endpoint tcp://0.0.0.0:5001 --agency.my-address=tcp://127.0.0.1:5001 --server.authentication false --agency.activate true --agency.size 3 --agency.endpoint tcp://127.0.0.1:5001 --agency.supervision true --database.directory agency1 &
arangod --server.endpoint tcp://0.0.0.0:5002 --agency.my-address=tcp://127.0.0.1:5002 --server.authentication false --agency.activate true --agency.size 3 --agency.endpoint tcp://127.0.0.1:5001 --agency.supervision true --database.directory agency2 &
arangod --server.endpoint tcp://0.0.0.0:5003 --agency.my-address=tcp://127.0.0.1:5003 --server.authentication false --agency.activate true --agency.size 3 --agency.endpoint tcp://127.0.0.1:5001 --agency.supervision true --database.directory agency3 &
```

If you are happy with a single agent, then simply use a single command like this:
```
arangod --server.endpoint tcp://0.0.0.0:5001 --server.authentication false --agency.activate true --agency.size 1 --agency.endpoint tcp://127.0.0.1:5001 --agency.supervision true --database-directory agency1 &
```

Furthermore, in the following sections when `--cluster.agency-address` is used multiple times to specify all three agent addresses, just use a single option ```--cluster.agency.address tcp://127.0.0.1:5001``` instead.


### Coordinators and DBServers

These two roles share a common set of relevant options. First you should specify the role using `--cluster.my-role`. This can either be `PRIMARY` (a database server) or `COORDINATOR`. Both need some unique information with which they will register in the agency, too. This could for example be some combination of host name and port or whatever you have at hand. However it must be unique for each instance and be provided as value for `--cluster.my-local-info`. Furthermore provide the external endpoint (IP and port) of the task via `--cluster.my-address`.

The following is a full-example of what it might look like:

```
arangod --server.authentication=false --server.endpoint tcp://0.0.0.0:8529 --cluster.my-address tcp://127.0.0.1:8529 --cluster.my-local-info db1 --cluster.my-role PRIMARY --cluster.agency-endpoint tcp://127.0.0.1:5001 --cluster.agency-endpoint tcp://127.0.0.1:5002 --cluster.agency-endpoint tcp://127.0.0.1:5003 --database.directory primary1 &
arangod --server.authentication=false --server.endpoint tcp://0.0.0.0:8530 --cluster.my-address tcp://127.0.0.1:8530 --cluster.my-local-info db2 --cluster.my-role PRIMARY --cluster.agency-endpoint tcp://127.0.0.1:5001 --cluster.agency-endpoint tcp://127.0.0.1:5002 --cluster.agency-endpoint tcp://127.0.0.1:5003 --database.directory primary2 &
arangod --server.authentication=false --server.endpoint tcp://0.0.0.0:8531 --cluster.my-address tcp://127.0.0.1:8531 --cluster.my-local-info coord1 --cluster.my-role COORDINATOR --cluster.agency-endpoint tcp://127.0.0.1:5001 --cluster.agency-endpoint tcp://127.0.0.1:5002 --cluster.agency-endpoint tcp://127.0.0.1:5003 --database.directory coordinator &
```

Note in particular that the endpoint descriptions given under `--cluster.my-address` and `--cluster.agency-endpoint` must not use the IP address `0.0.0.0` because they must contain an actual address that can be routed to the corresponding server. The `0.0.0.0` in `--server.endpoint` simply means that the server binds itself to all available network devices with all available IP addresses.

Upon registering with the agency during startup the cluster will assign an ID to every task. The generated ID will be printed out to the log or can be accessed via the http API by calling `http://server-address/_admin/server/id`.
Should you ever have to restart a task, simply reuse the same value for `--cluster.my-local-info` and the same ID will be picked.

You have now launched a complete ArangoDB cluster and can contact its coordinator at the endpoint `tcp://127.0.0.1:8531`, which means that you can reach the web UI under `http://127.0.0.1:8531`.
