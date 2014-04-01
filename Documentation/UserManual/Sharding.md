Sharding {#Sharding}
====================

@NAVIGATE_Sharding
@EMBEDTOC{ShardingTOC}

The following sections will give you all the technical details of the
sharding setup. If you just want to set up a cluster, follow the 
instructions given in @ref CookbookCluster.

Introduction {#ShardingIntroduction}
====================================

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
and to synchronise reconfiguration and fail-over operations we currently
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
configuration whatsoever is needed and you can organise authentication
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
dispatcher, see ??? for details.


How to try it out {#ShardingTryItOut}
=====================================

In this text we assume that you are working with a standard installation
of ArangoDB with at least a version number of 2.0. This means that everything
is compiled for cluster operation, that `etcd` is compiled and
the executable is installed in the location mentioned in the
configuration file. The first step is to switch on the dispatcher
functionality in your configuration of `arangod`. In order to do this, change
the `cluster.disable-dispatcher-kickstarter` and
`cluster.disable-dispatcher-interface` options in `arangod.conf` both
to `false`.

Note that once you switch `cluster.disable-dispatcher-interface` to
`false`, the usual web front end is automatically replaced with the
web front end for cluster planning. Therefore you can simply point
your browser to `http://localhost:8529` (if you are running on the
standard port) and you are guided through the planning and launching of
a cluster with a graphical user interface. Alternatively, you can follow
the instructions below to do the same on the command line interface.

We will first plan and launch a cluster, such that all your servers run
on the local machine.

Start up a regular ArangoDB, either in console mode or connect to it with 
the Arango shell `arangosh`. Then you can ask it to plan a cluster for
you:

    arangodb> var Planner = require("org/arangodb/cluster").Planner;
    arangodb> p = new Planner({numberOfDBservers:3, numberOfCoordinators:2});
    [object Object]

If you are curious you can look at the plan of your cluster:

    arangodb> p.getPlan();

This will show you a huge JSON document. More interestingly, some further
components tell you more about the layout of your cluster:

    arangodb> p.DBservers;
    [ 
      { 
        "id" : "Pavel", 
        "dispatcher" : "me", 
        "port" : 8629 
      }, 
      { 
        "id" : "Perry", 
        "dispatcher" : "me", 
        "port" : 8630 
      }, 
      { 
        "id" : "Pancho", 
        "dispatcher" : "me", 
        "port" : 8631 
      } 
    ]
    
    arangodb> p.coordinators;
    [ 
      { 
        "id" : "Claus", 
        "dispatcher" : "me", 
        "port" : 8530 
      }, 
      { 
        "id" : "Chantalle", 
        "dispatcher" : "me", 
        "port" : 8531 
      } 
    ]

This tells you the ports on which your ArangoDB processes will listen.
We will need the 8530 (or whatever appears on your machine) for the
coordinators below.

More interesting is that such a cluster plan document can be used to
start up the cluster conveniently using a `Kickstarter` object. Please
note that the `launch` method of the kickstarter shown below initialises
all data directories and log files, so if you have previously used the
same cluster plan you will lose all your data. Use the `relaunch` method
described below instead in that case.

    arangodb> var Kickstarter = require("org/arangodb/cluster").Kickstarter;
    arangodb> k = new Kickstarter(p.getPlan());
    arangodb> k.launch();

That is all you have to do, to fire up your first cluster. You will see some
output, which you can safely ignore (as long as no error happens). 

From that point on, you can contact one of the coordinators and use the cluster
as if it were a single ArangoDB instance (use the port number from above
instead of 8530, if you get a different one) (probably from another
shell window):

    $ arangosh --server.endpoint tcp://localhost:8530
    [... some output omitted]
    arangosh [_system]> db._listDatabases();
    [ 
      "_system" 
    ]

This for example, lists the cluster wide databases. 

Now, let us create a sharded collection. Note, that we only have to specify 
the number of shards to use in addition to the usual command. 
The shards are automatically distributed among your DBservers:

    arangosh [_system]> example = db._create("example",{numberOfShards:6});
    [ArangoCollection 1000001, "example" (type document, status loaded)]
    arangosh [_system]> x = example.save({"name":"Hans", "age":44});
    { 
      "error" : false, 
      "_id" : "example/1000008", 
      "_rev" : "13460426", 
      "_key" : "1000008" 
    }
    arangosh [_system]> example.document(x._key);
    { 
      "age" : 44, 
      "name" : "Hans", 
      "_id" : "example/1000008", 
      "_rev" : "13460426", 
      "_key" : "1000008" 
    }

You can shut down your cluster by using the following Kickstarter
method (in the ArangoDB console):

    arangodb> k.shutdown();

If you want to start your cluster again without losing data you have
previously stored in it, you can use the `relaunch` method in exactly the
same way as you previously used the `launch` method:

    arangodb> k.relaunch();

You can now check, whether or not, all your cluster processes are still
running, by issuing:

    arangodb> k.isHealthy();

This will show you the status of all processes in the cluster. You
should see "RUNNING" there, in all the relevant places.

Finally, to clean up the whole cluster (losing all the data stored in
it), do:

    arangodb> k.shutdown();
    arangodb> k.cleanup();

We conclude this section with another example using two machines, which 
will act as two dispatchers. We start from scratch using two machines,
running on the network addresses `tcp://192.168.173.78:8529` and
`tcp://192.168.173.13:6789`. Both need to have a regular ArangoDB
instance installed and running. Please make sure, that both bind to
all network devices, so that they can talk to each other. Also enable
the dispatcher functionality on both of them, as described above.

    arangodb> var Planner = require("org/arangodb/cluster").Planner;
    arangodb> var p = new Planner({
         dispatchers: {"me":{"endpoint":"tcp://192.168.173.78:8529"},
                       "theother":{"endpoint":"tcp://192.168.173.13:6789"}}, 
         "numberOfCoordinators":2, "numberOfDBservers": 2});

With these commands, you create a cluster plan involving two machines. 
The planner will put one DBserver and one Coordinator on each machine. 
You can now launch this cluster exactly as explained earlier:

    arangodb> var Kickstarter = require("org/arangodb/cluster").Kickstarter;
    arangodb> k = new Kickstarter(p.getPlan());
    arangodb> k.launch();

Likewise, the methods `shutdown`, `relaunch`, `isHealthy` and `cleanup`
work exactly as in the single server case.

See @ref JSModuleCluster "the corresponding chapter of the reference manual"
for detailed information about the `Planner` and `Kickstarter` classes.


Status of the implementation {#ShardingStatusImpl}
==================================================

This version 2.0 of ArangoDB contains the first usable implementation
of the sharding extensions. However, not all planned features are
included in this release. In particular, automatic fail-over is fully
prepared in the architecture but is not yet implemented. If you use
Version 2.0 in cluster mode in a production system, you have to
organise failure recovery manually. This is why, at this stage with
Version 2.0 we do not yet recommend to use the cluster mode in
production systems. If you really need this feature now, please contact
us.

This section provides an overview over the implemented and future
features.

In normal single instance mode, ArangoDB works as usual
with the same performance and functionality as in previous releases.

In cluster mode, the following things are implemented in version 2.0 
and work:

  - All basic CRUD operations for single documents and edges work
    essentially with good performance.
  - One can use sharded collections and can configure the number of
    shards for each such collection individually. In particular, one
    can have fully sharded collections as well as cluster-wide available
    collections with only a single shard. After creation, these
    differences are transparent to the client.
  - Creating and dropping cluster-wide databases works.
  - Creating, dropping and modifying cluster-wide collections all work.
    Since these operations occur seldom, we will only improve their
    performance in a future release, when we will have our own
    implementation of the agency as well as a cluster-wide event managing
    system (see road map for release 2.3).
  - Sharding in a collection, can be configured to use hashing
    on arbitrary properties of the documents in the collection.
  - Creating and dropping indices on sharded collections works. Please
    note that an index on a sharded collection is not a global index 
    but only leads to a local index of the same type on each shard.
  - All SimpleQueries work. Again, we will improve the performance in
    future releases, when we revisit the AQL query optimiser 
    (see road map for release 2.2).
  - AQL queries work, but with relatively bad performance. Also, if the
    result of a query on a sharded collection is large, this can lead
    to an out of memory situation on the coordinator handling the
    request. We will improve this situation when we revisit the AQL
    query optimiser (see road map for release 2.2).
  - Authentication on the cluster works with the method known from
    single ArangoDB instances on the coordinators. A new cluster-internal
    authorisation scheme has been created. See below for hints on a
    sensible firewall and authorisation setup.
  - Most standard API calls of the REST interface work on the cluster
    as usual, with a few exceptions, which do no longer make sense on
    a cluster or are harder to implement. See below for details.


The following does not yet work, but is planned for future releases (see
road map):

  - Transactions can be run, but do not behave like transactions. They
    simply execute but have no atomicity or isolation in version 2.0.
    See the road map for version 2.X.
  - We plan to revise the AQL optimiser for version 2.2. This is
    necessary since for efficient queries in cluster mode we have to
    do as much as possible of the filtering and sorting on the
    individual DBservers rather than on the coordinator.
  - Our software architecture is fully prepared for replication, automatic 
    fail-over and recovery of a cluster, which will be implemented
    for version 2.3 (see our road map).
  - This setup will at the same time, allow for hot swap and in-service 
    maintenance and scaling of a cluster. However, in version 2.0 the
    cluster layout is static and no redistribution of data between the
    DBservers or moving of shards between servers is possible. 
  - For version 2.3 we envision an extension for AQL to allow writing
    queries. This will also allow to run modifying queries in parallel
    on all shards.
  - At this stage the sharding of an edge collection is independent of
    the sharding of the corresponding vertex collection in a graph.
    For version 2.2 we plan to synchronise the two, to allow for more
    efficient graph traversal functions in large, sharded graphs. We
    will also do research on distributed algorithms for graphs and
    implement new algorithms in ArangoDB. However, at this stage, all
    graph traversal algorithms are executed on the coordinator and
    this means relatively poor performance since every single edge
    step leads to a network exchange.
  - In version 2.0 the import API is broken for sharded collections.
    It will appear to work but will in fact silently fail. Fixing this
    is on the road map for version 2.1.
  - In version 2.0 the `arangodump` and `arangorestore` programs
    can not be used talking to a coordinator to directly backup
    sharded collections. At this stage, one has to backup the
    DBservers individually using `arangodump` and `arangorestore`
    on them. The coordinators themselves do not hold any state and
    therefore do not need backup. Do not forget to backup the meta
    data stored in the agency because this is essential to access
    the sharded collections. These limitations will be fixed in
    version 2.1.
  - In version 2.0 the replication API (`/_api/replication`)
    does not work on coordinators. This is intentional, since the 
    plan is to organise replication with automatic fail-over directly
    on the DBservers, which is planned for version 2.3.
  - The `db.<collection>.rotate()` method for sharded collections is not
    yet implemented, but will be supported from version 2.1 onwards.
  - The `db.<collection>.rename()` method for sharded collections is not
    yet implemented, but will be supported from version 2.1 onwards.
  - The `db.<collection>.checksum()` method for sharded collections is
    not yet implemented, but will be supported from version 2.1
    onwards.

The following restrictions will probably stay, for cluster mode, even in
future versions. This is, because they are difficult or even impossible
to implement efficiently:

  - Custom key generators with the `keyOptions` property in the
    `_create` method for collections are not supported. We plan
    to improve this for version 2.1 (see road map). However, due to the
    distributed nature of a sharded collection, not everything that is
    possible in the single instance situation will be possible on a
    cluster. For example the auto-increment feature in a cluster with
    multiple DBservers and coordinators would have to lock the whole
    collection centrally for every document creation, which
    essentially defeats the performance purpose of sharding.
  - Unique constraints on non-sharding keys are unsupported. The reason
    for this is that we do not plan to have global indices for sharded
    collections. Therefore, there is no single authority that could
    efficiently decide whether or not the unique constraint is
    satisfied by a new document. The only possibility would be to have
    a central locking mechanism and use heavy communication for every
    document creation to ensure the unique constraint.
  - The method `db.<collection>.revision()` for a sharded collection 
    returns the highest revision number from all shards. However,
    revision numbers are assigned per shard, so this is not guaranteed
    to be the revision of the latest inserted document. Again,
    maintaining a global revision number over all shards is very
    difficult to maintain efficiently.
  - The methods `db.<collection>.first()` and `db.<collection>.last()` are 
    unsupported for collections with more than one shard. The reason for
    this, is that temporal order in a highly parallelised environment
    like a cluster is difficult or even impossible to achieve
    efficiently. In a cluster it is entirely possible that two
    different coordinators add two different documents to two
    different shards *at the same time*. In such a situation it is not
    even well-defined which of the two documents is "later". The only
    way to overcome this fundamental problem would again be a central
    locking mechanism, which is not desirable for performance reasons.
  - Contrary to the situation in a single instance, objects representing
    sharded collections are broken after their database is dropped.
    In a future version they might report that they are broken, but
    it is not feasible and not desirable to retain the cluster database 
    in the background until all collection objects are garbage
    collected.
  - In a cluster, the automatic creation of collections on a call to
    `db._save(ID)` is not supported. This is because one would have no
    way to specify the number or distribution of shards for the newly
    created collection. Therefore we will not offer this feature for
    cluster mode.


Authentication in a cluster {#ShardingAuthentication}
=====================================================

In this section we describe, how authentication in a cluster is done
properly. For experiments it is possible to run the cluster completely
unauthorised by using the option `--server.disable-authentication true`
on the command line or the corresponding entry in the configuration
file. However, for production use, this is not desirable.

You can turn on authentication in the cluster by switching it on in the
configuration of your dispatchers. When you now use the planner and
kickstarter to create and launch a cluster, the `arangod` processes in
your cluster will automatically run with authentication, exactly as the
dispatchers themselves. However, the cluster will have a sharded
collection `_users` with one shard containing only the user `root` with
an empty password. We emphasise that this sharded cluster-wide
collection is different from the `_users` collections in each
dispatcher!

The coordinators in your cluster will use this cluster-wide sharded collection
to authenticate HTTP requests. If you add users using the usual methods
via a coordinator, you will in fact change the cluster-wide
collection `_users` and thus all coordinators will eventually see the
new users and authenticate against them. "Eventually" means that they
might need a few seconds to notice the change in user setup and update
their user cache.

The DBservers will have their authentication switched on as well.
However, they do not use the cluster-wide `_users` collection for
authentication, because the idea is, that the outside clients do not talk
to the DBservers directly, but always go via the coordinators. For the
cluster-internal communication between coordinators and DBservers (in
both directions), we use a simpler setup: There are two new
configuration options `cluster.username` and `cluster.password`, which
default to `root` and the empty password `""`. If you want to deviate
from this default you have to change these two configuration options
in all configuration files on all machines in the cluster. This just
means that you have to set these two options to the same values in all
configuration files `arangod.conf` in all dispatchers, since the
coordinators and DBservers will simply inherit this configuration file
from the dispatcher that has launched them.

Let us summarise what you have to do, to enable authentication in a cluster:

  1. Set `server.disable-authentication` to `false` in all configuration
     files of all dispatchers (this is already the default).
  2. Put the same values for `cluster.username` and `cluster.password`
     in the very same configuration files of all dispatchers.
  3. Create users via the usual interface on the coordinators
     (initially after the cluster launch there will be a single user `root`
     with empty password).

Please note, that in Version 2.0 of ArangoDB you can already configure the 
endpoints of the coordinators to use SSL. However, this is not yet conveniently
supported in the planner, kickstarter and in the graphical cluster 
management tools. We will fix this in the next version.

Please also consider the comments in the following section about
firewall setup.


Recommended firewall setup {#ShardingFirewallSetup}
===================================================

This section is intended for people who run a cluster in production
systems.

The whole idea of the cluster setup is that the coordinators serve HTTP
requests to the outside world and that all other processes (DBservers
and agency) are only available from within the cluster itself.
Therefore, in a production environment, one has to put the whole cluster
behind a firewall and only open the ports to the coordinators to the
client processes. 

Note however that for the asynchronous cluster-internal communication, 
the DBservers perform HTTP requests to the coordinators, which means
that the coordinators must also be reachable from within the cluster.

Furthermore, it is of the utmost importance to hide the agent processes of
the agency behind the firewall, since, at least at this stage, requests
to them are completely unauthorised. Leaving their ports exposed to
the outside world, endangers all data in the cluster, because everybody
on the internet could make the cluster believe that, for example, you wanted 
your databases dropped! This weakness will be alleviated in future versions,
because we will replace `etcd` by our own specialised agency
implementation, which will allow for authentication.

A further comment applies to the dispatchers. Usually you will open the
HTTP endpoints of your dispatchers to the outside world and switch on
authentication for them. This is necessary to contact them from the
outside, in the cluster launch phase. However, actually you only
need to contact one of them, who will then in turn contact the others
using cluster-internal communication. You can even get away with closing
access to all dispatchers to the outside world, provided the machine
running your browser is within the cluster network and does not have to
go through the firewall to contact the dispatchers. It is important to
be aware that anybody who can reach a dispatcher and can authorise
himself to it can launch arbitrary processes on the machine on which
the dispatcher runs!

Therefore we recommend to use SSL endpoints with user/password 
authentication on the dispatchers *and* to block access to them in
the firewall. You then have to launch the cluster using an `arangosh`
or browser running within the cluster.


@NAVIGATE_Sharding
