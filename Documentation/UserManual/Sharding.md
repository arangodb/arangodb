Sharding {#Sharding}
====================

@NAVIGATE_Sharding
@EMBEDTOC{ShardingTOC}

Introduction {#ShardingIntroduction}
====================================

Sharding allows to use multiple machines to run a cluster of ArangoDB
instances that together appear to be a single database. This enables
you to store much more data, since ArangoDB distributes the data 
automatically to the different servers.

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


How to try it out {#ShardingTryItOut}
=====================================

In this text we assume that ArangoDB is compiled for cluster operation
and that `etcd` is compiled and the executable is installed in the
`bin` directory of your ArangoDB installation.

We will first launch a cluster, such that all your servers run on the
local machine.

Start up a regular ArangoDB in console mode. Then you can ask it to
plan a cluster for you:

    arangodb> var Planner = require("org/arangodb/cluster/planner").Planner;
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

This for example tells you the ports on which your ArangoDB processes
will run. We will need the 8530 (or whatever appears on your machine) below.

More interesting is that such a cluster plan document can be used to
start up the cluster conveniently using a `Kickstarter` object:

    arangodb> var Kickstarter = require("org/arangodb/cluster/kickstarter").Kickstarter;
    arangodb> k = new Kickstarter(p.getPlan());
    arangodb> k.launch();

is all you have to do to fire up your first cluster. You will see some
output, which you can safely ignore (as long as no error happens). 
From then on you
can contact one of the coordinators and use the cluster as if it were
a single ArangoDB instance (use the port number from above instead of
8530, if you get a different one) (probably from another shell window):

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

    arangosh> k.shutdown();


Status of the implementation {#ShardingStatusImpl}
==================================================

At this stage all the basic CRUD operation for single documents or edges
and all those simple queries that do not need an index are implemented.
Creating collections, dropping collections, creating databases,
dropping databases works, however these operations are relatively 
slow at this stage. This is mostly due to the `etcd` program we are
currently using. We will probably see a speedup in a later release.
The basic CRUD operations already have a relatively good performance,
the simple queries are in need of further optimisations as well.

See @ref JSModuleCluster "the corresponding chapter of the reference manual"
for detailed information about the `Planner` and `Kickstarter` classes.

@NAVIGATE_Sharding
