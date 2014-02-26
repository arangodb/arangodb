Sharding {#Sharding}
====================

@NAVIGATE_Sharding
@EMBEDTOC{ShardingTOC}

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
and to synchronise reconfiguration and failover operations we currently
use a an external program called `etcd`. It provides a hierarchical
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
relatively big datastructure in JSON format. You can then use this
cluster plan to startup, shutdown, check and cleanup your cluster.

This latter phase uses so-called "dispatchers". A dispatcher is yet another
ArangoDB instance and you have to install exactly one such instance on
every machine that will take part in your cluster. No special
configuration whatsoever is needed and you can organise authentication
exactly as you would in a normal ArangoDB instance.

However, you can use any of these dispatchers to plan and start your
cluster. In the planning phase, you have tell the planner about all
dispatchers in your cluster and it will automatically distribute your
agency, DBserver and coordinator processes amongst the dispatchers. The
result is the cluster plan which you feed into the kickstarter. The
kickstarter is a program that actually uses the dispatchers to
manipulate the processes in your cluster. It runs on one of the
dispatchers, which analyses the cluster plan and executes those actions,
for which it is personally responsible, and forwards all other actions
to the corresponding dispatcher. This is possible, because the cluster
plan incorporates the information about all dispatchers.

We also offer a graphical user interface to the cluster planner and
dispatcher, see ??? for details.


How to try it out {#ShardingTryItOut}
=====================================

In this text we assume that you are working with a standard installation
of ArangoDB with version at least 2.0. This means that everything
is compiled for cluster operation and that `etcd` is compiled and
the executable is installed in the same place as your `arangod`
executable.

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

will show you a huge JSON document. More interestingly, some further
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

is all you have to do to fire up your first cluster. You will see some
output, which you can safely ignore (as long as no error happens). 

From then on you can contact one of the coordinators and use the cluster
as if it were a single ArangoDB instance (use the port number from above
instead of 8530, if you get a different one) (probably from another
shell window):

    $ arangosh --server.endpoint tcp://localhost:8530
    [... some output omitted]
    arangosh [_system]> db._listDatabases();
    [ 
      "_system" 
    ]

This for example lists the cluster wide databases. Now let us create a
sharded collection, we only have to specify the number of shards to use
in addition to the usual command. The shards are automatically
distributed amongst your DBservers:

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
method (in the original ArangoDB console):

    arangodb> k.shutdown();

If you want to start your cluster again without losing the data you have
previosly stored in it, you can use the `relaunch` method in exactly the
same way as you previously used the `launch` method:

    arangodb> k.relaunch();

You can now check whether or not all your cluster processes are still
running by issueing:

    arangodb> k.isHealthy();

which will show you the status of all the processes in the cluster. You
should see "RUNNING" there in all relevant places.

Finally, to clean up the whole cluster (losing all the data stored in
it), do:

    arangodb> k.shutdown();
    arangodb> k.cleanup();

We conclude this section with another example using two machines, that
is two different dispatchers. We start from scratch using two machines,
running on the network addresses `tcp://192.168.173.78:8529` and
`tcp://192.168.173.13:6789`, say. Both need to have a regular ArangoDB
instance installed and running, please make sure that both bind to
all network devices, such that they can talk to each other.

    arangodb> var Planner = require("org/arangodb/cluster").Planner;
    arangodb> var p = new Planner({
         dispatchers: {"me":{"endpoint":"tcp://192.168.173.78:8529"},
                       "theother":{"endpoint":"tcp://192.168.173.13:6789"}}, 
         "numberOfCoordinators":2, "numberOfDBservers": 2});

With this you create a cluster plan involving two machines. The planner
will put one DBserver and one coordinator on each machine. You can now
launch this cluster exactly as above:

    arangodb> var Kickstarter = require("org/arangodb/cluster").Kickstarter;
    arangodb> k = new Kickstarter(p.getPlan());
    arangodb> k.launch();

Likewise, the methods `shutdown`, `relaunch`, `isHealthy` and `cleanup`
work exactly as in the single server case.

See @ref JSModuleCluster "the corresponding chapter of the reference manual"
for detailed information about the `Planner` and `Kickstarter` classes.

Status of the implementation {#ShardingStatusImpl}
==================================================

At this stage all the basic CRUD operation for single documents or edges
and all simple queries are implemented. AQL queries work but not with
the best possible performance.
Creating collections, dropping collections, creating databases,
dropping databases all work, however these operations are relatively 
slow at this stage. This is mostly due to the `etcd` program we are
currently using. We will probably see a speedup in a later release.
The basic CRUD operations already have a relatively good performance,
the simple queries are in need of further optimisations as well.

The following does not yet work:

  - Transactions
  - optimised AQL
  - writing AQL/parallel
  - replication/automatic failover
  - hot swap/maintenance of cluster
  - resizing cluster, moving shards
  - sharding of edge collections is currently independent from the one
    on vertex collections
  - no distributed graph algorithms
  - Custom key generators:
    db._create("...", { keyOptions: { ... } })
  - unique constraints on non-shard keys are unsupported
    db.ensureIndex(): 
  - import API is broken for sharded collections (will not work, but
    there is no error ATM)
  - db.<collection>.revision() returns the highest revision number from
    all shards. But revision numbers are assigned per shard, so this is
    not guaranteed to be the revision of the latest inserted document.
  - db.<collection>.first() and db.<collection>.last() are unsupported
    for collections with more than one shard.
  - collections objects are broken if their database is dropped
  - db.<collection>.rotate() not implemented
  - db.<collection>.rename() not implemented
  - db.<collection>.checksum() is currently unsupported for sharded
    collections
  - automatic creation of collections on db._save(ID) not supported


Recommended firewall setup
==========================



@NAVIGATE_Sharding
