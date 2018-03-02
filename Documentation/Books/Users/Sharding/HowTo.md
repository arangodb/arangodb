How to try it out
=================

In this text we assume that you are working with a standard installation
of ArangoDB with at least a version number of 2.0. This means that everything
is compiled for cluster operation, that *etcd* is compiled and
the executable is installed in the location mentioned in the
configuration file. The first step is to switch on the dispatcher
functionality in your configuration of *arangod*. In order to do this, change
the *cluster.disable-dispatcher-kickstarter* and
*cluster.disable-dispatcher-interface* options in *arangod.conf* both
to *false*.

**Note**: Once you switch *cluster.disable-dispatcher-interface* to
*false*, the usual web front end is automatically replaced with the
web front end for cluster planning. Therefore you can simply point
your browser to *http://localhost:8529* (if you are running on the
standard port) and you are guided through the planning and launching of
a cluster with a graphical user interface. Alternatively, you can follow
the instructions below to do the same on the command line interface.

We will first plan and launch a cluster, such that all your servers run
on the local machine.

Start up a regular ArangoDB, either in console mode or connect to it with 
the Arango shell *arangosh*. Then you can ask it to plan a cluster for
you:

```js
arangodb> var Planner = require("org/arangodb/cluster").Planner;
arangodb> p = new Planner({numberOfDBservers:3, numberOfCoordinators:2});
[object Object]
```

If you are curious you can look at the plan of your cluster:

```
arangodb> p.getPlan();
```

This will show you a huge JSON document. More interestingly, some further
components tell you more about the layout of your cluster:

```js
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
```

This tells you the ports on which your ArangoDB processes will listen.
We will need the 8530 (or whatever appears on your machine) for the
coordinators below.

More interesting is that such a cluster plan document can be used to
start up the cluster conveniently using a *Kickstarter* object. Please
note that the *launch* method of the kickstarter shown below initializes
all data directories and log files, so if you have previously used the
same cluster plan you will lose all your data. Use the *relaunch* method
described below instead in that case.

```js
arangodb> var Kickstarter = require("org/arangodb/cluster").Kickstarter;
arangodb> k = new Kickstarter(p.getPlan());
arangodb> k.launch();
```js

That is all you have to do, to fire up your first cluster. You will see some
output, which you can safely ignore (as long as no error happens). 

From that point on, you can contact one of the coordinators and use the cluster
as if it were a single ArangoDB instance (use the port number from above
instead of 8530, if you get a different one) (probably from another
shell window):

```js
$ arangosh --server.endpoint tcp://localhost:8530
[... some output omitted]
arangosh [_system]> db._listDatabases();
[ 
  "_system" 
]
```js
This for example, lists the cluster wide databases. 

Now, let us create a sharded collection. Note, that we only have to specify 
the number of shards to use in addition to the usual command. 
The shards are automatically distributed among your DBservers:

```js
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
```js

You can shut down your cluster by using the following Kickstarter
method (in the ArangoDB console):

```js
arangodb> k.shutdown();
```

If you want to start your cluster again without losing data you have
previously stored in it, you can use the *relaunch* method in exactly the
same way as you previously used the *launch* method:

```js
arangodb> k.relaunch();
```

**Note**: If you have destroyed the object *k* for example because you
have shutdown the ArangoDB instance in which you planned the cluster,
then you can reproduce it for a *relaunch* operation, provided you have
kept the cluster plan object provided by the *getPlan* method. If you
had for example done:

```js
arangodb> var plan = p.getPlan();
arangodb> require("fs").write("saved_plan.json",JSON.stringify(plan));
```

Then you can later do (in another session):

```js
arangodb> var plan = require("fs").read("saved_plan.json");
arangodb> plan = JSON.parse(plan);
arangodb> var Kickstarter = require("org/arangodb/cluster").Kickstarter;
arangodb> var k = new Kickstarter(plan);
arangodb> k.relaunch();
```

to start the existing cluster anew.

You can check, whether or not, all your cluster processes are still
running, by issuing:

```js
arangodb> k.isHealthy();
```

This will show you the status of all processes in the cluster. You
should see "RUNNING" there, in all the relevant places.

Finally, to clean up the whole cluster (losing all the data stored in
it), do:

```js
arangodb> k.shutdown();
arangodb> k.cleanup();
```

We conclude this section with another example using two machines, which 
will act as two dispatchers. We start from scratch using two machines,
running on the network addresses *tcp://192.168.173.78:8529* and
*tcp://192.168.173.13:6789*. Both need to have a regular ArangoDB
instance installed and running. Please make sure, that both bind to
all network devices, so that they can talk to each other. Also enable
the dispatcher functionality on both of them, as described above.

```js
arangodb> var Planner = require("org/arangodb/cluster").Planner;
arangodb> var p = new Planner({
    dispatchers: {"me":{"endpoint":"tcp://192.168.173.78:8529"},
                  "theother":{"endpoint":"tcp://192.168.173.13:6789"}}, 
    "numberOfCoordinators":2, "numberOfDBservers": 2});
```

With these commands, you create a cluster plan involving two machines. 
The planner will put one DBserver and one Coordinator on each machine. 
You can now launch this cluster exactly as explained earlier:

```js
    arangodb> var Kickstarter = require("org/arangodb/cluster").Kickstarter;
    arangodb> k = new Kickstarter(p.getPlan());
    arangodb> k.launch();
```

Likewise, the methods *shutdown*, *relaunch*, *isHealthy* and *cleanup*
work exactly as in the single server case.

See [the corresponding chapter of the reference manual](../ModulePlanner/README.md)
for detailed information about the *Planner* and *Kickstarter* classes.

