!CHAPTER Sharding

Sharding allows to use multiple machines to run a cluster of ArangoDB
instances that together constitute a single database. This enables
you to store much more data, since ArangoDB distributes the data 
automatically to the different servers. In many situations one can 
also reap a benefit in data throughput, again because the load can
be distributed to multiple machines.

In a cluster there are essentially two types of processes: "DBservers"
and "coordinators". The former actually store the data, the latter
expose the database to the outside world. The clients talk to the
coordinators exactly as they would talk to a single ArangoDB instance 
via the REST interface. The coordinators know about the configuration of 
the cluster and automatically forward the incoming requests to the
right DBservers.

As a central highly available service to hold the cluster configuration
and to synchronize reconfiguration and fail-over operations we currently
use a an external program called `etcd` (see [github
page](https://github.com/coreos/etcd)). It provides a hierarchical
key value store with strong consistency and reliability promises.
This is called the "agency" and its processes are called "agents".

All this is admittedly a relatively complicated setup and involves a lot
of steps for the startup and shutdown of clusters. Therefore we have created
convenience functionality to plan, setup, start and shutdown clusters.

The whole process works in two phases, first the "planning" phase and
then the "running" phase. In the planning phase it is decided which
processes with which roles run on which machine, which ports they use,
where the central agency resides and what ports its agents use. The
result of the planning phase is a "cluster plan", which is just a
relatively big data structure in JSON format. You can then use this
cluster plan to startup, shutdown, check and cleanup your cluster.

This latter phase uses so-called "dispatchers". A dispatcher is yet another
ArangoDB instance and you have to install exactly one such instance on
every machine that will take part in your cluster. No special
configuration whatsoever is needed and you can organize authentication
exactly as you would in a normal ArangoDB instance. You only have
to activate the dispatcher functionality in the configuration file
(see options `cluster.disable-dispatcher-kickstarter` and
`cluster.disable-dispatcher-interface`, which are both initially
set to `true` in the standard setup we ship).

However, you can use any of these dispatchers to plan and start your
cluster. In the planning phase, you have to tell the planner about all
dispatchers in your cluster and it will automatically distribute your
agency, DBserver and coordinator processes amongst the dispatchers. The
result is the cluster plan which you feed into the kickstarter. The
kickstarter is a program that actually uses the dispatchers to
manipulate the processes in your cluster. It runs on one of the
dispatchers, which analyses the cluster plan and executes those actions,
for which it is personally responsible, and forwards all other actions
to the corresponding dispatchers. This is possible, because the cluster
plan incorporates the information about all dispatchers.

We also offer a graphical user interface to the cluster planner and
dispatcher.

