Cluster Architecture
====================

The Cluster architecture of ArangoDB is a _CP_ master/master model with no
single point of failure. 

With "CP" in terms of the [CAP theorem](https://en.wikipedia.org/wiki/CAP_theorem)
we mean that in the presence of a
network partition, the database prefers internal consistency over
availability. With "master/master" we mean that clients can send their
requests to an arbitrary node, and experience the same view on the
database regardless. "No single point of failure" means that the cluster
can continue to serve requests, even if one machine fails completely.

In this way, ArangoDB has been designed as a distributed multi-model
database. This section gives a short outline on the Cluster architecture and
how the above features and capabilities are achieved.

Structure of an ArangoDB Cluster
--------------------------------

An ArangoDB Cluster consists of a number of ArangoDB instances
which talk to each other over the network. They play different roles,
which will be explained in detail below. 

The current configuration
of the Cluster is held in the _Agency_, which is a highly-available
resilient key/value store based on an odd number of ArangoDB instances
running [Raft Consensus Protocol](https://raft.github.io/).

For the various instances in an ArangoDB Cluster there are three distinct
roles:

- _Agents_
- _Coordinators_
- _DBServers_.

In the following sections we will shed light on each of them.

![ArangoDB Cluster](cluster_topology.png)

### Agents

One or multiple _Agents_ form the _Agency_ in an ArangoDB Cluster. The
_Agency_ is the central place to store the configuration in a Cluster. It
performs leader elections and provides other synchronization services for
the whole Cluster. Without the _Agency_ none of the other components can
operate.

While generally invisible to the outside the _Agency_ is the heart of the
Cluster. As such, fault tolerance is of course a must have for the
_Agency_. To achieve that the _Agents_ are using the [Raft Consensus
Algorithm](https://raft.github.io/). The algorithm formally guarantees
conflict free configuration management within the ArangoDB Cluster.

At its core the _Agency_ manages a big configuration tree. It supports
transactional read and write operations on this tree, and other servers
can subscribe to HTTP callbacks for all changes to the tree.

### Coordinators

_Coordinators_ should be accessible from the outside. These are the ones
the clients talk to. They will coordinate cluster tasks like
executing queries and running Foxx services. They know where the
data is stored and will optimize where to run user supplied queries or
parts thereof. _Coordinators_ are stateless and can thus easily be shut down
and restarted as needed.

### DBServers

_DBservers_ are the ones where the data is actually hosted. They
host shards of data and using synchronous replication a _DBServer_ may
either be _leader_ or _follower_ for a shard. Document operations are first
applied on the _leader_ and then synchronously replicated to 
all followers.

Shards must not be accessed from the outside but indirectly through the
_Coordinators_. They may also execute queries in part or as a whole when
asked by a _Coordinator_.

See [Sharding](#sharding) below for more information.

Many sensible configurations
----------------------------

This architecture is very flexible and thus allows many configurations,
which are suitable for different usage scenarios:

 1. The default configuration is to run exactly one _Coordinator_ and
    one _DBServer_ on each machine. This achieves the classical
    master/master setup, since there is a perfect symmetry between the
    different nodes, clients can equally well talk to any one of the
    _Coordinators_ and all expose the same view to the data store. _Agents_
    can run on separate, less powerful machines.
 2. One can deploy more _Coordinators_ than _DBservers_. This is a sensible
    approach if one needs a lot of CPU power for the Foxx services,
    because they run on the _Coordinators_.
 3. One can deploy more _DBServers_ than _Coordinators_ if more data capacity
    is needed and the query performance is the lesser bottleneck
 4. One can deploy a _Coordinator_ on each machine where an application
    server (e.g. a node.js server) runs, and the _Agents_ and _DBServers_
    on a separate set of machines elsewhere. This avoids a network hop
    between the application server and the database and thus decreases
    latency. Essentially, this moves some of the database distribution
    logic to the machine where the client runs.

As you can see, the _Coordinator_ layer can be scaled and deployed independently
from the _DBServer_ layer.

{% hint 'warning' %}
It is a best practice and a recommended approach to run _Agent_ instances
on different machines than _DBServer_ instances.

When deploying using the tool [_Starter_](../../../Deployment/ArangoDBStarter/README.md)
this can be achieved by using the options `--cluster.start-dbserver=false` and
`--cluster.start-coordinator=false` on the first three machines where the _Starter_
is started, if the desired _Agency_ _size_ is 3, or on the first 5 machines
if the desired _Agency_ _size_ is 5.
{% endhint %}

{% hint 'info' %}
The different instances that form a Cluster are supposed to be run in the same
_Data Center_ (DC), with reliable and high-speed network connection between
all the machines participating to the Cluster.

Multi-datacenter Clusters, where the entire structure and content of a Cluster located
in a specific DC is replicated to others Clusters located in different DCs, are
possible as well. See [Datacenter to datacenter replication](../DC2DC/README.md)
(DC2DC) for further details.
{% endhint %}

Cluster ID
----------

Every non-Agency ArangoDB instance in a Cluster is assigned a unique
ID during its startup. Using its ID a node is identifiable
throughout the Cluster. All cluster operations will communicate
via this ID.

Sharding
--------

Using the roles outlined above an ArangoDB Cluster is able to distribute
data in so called _shards_ across multiple _DBServers_. From the outside
this process is fully transparent and as such we achieve the goals of
what other systems call "master-master replication".

In an ArangoDB Cluster you talk to any _Coordinator_ and whenever you read or write data
it will automatically figure out where the data is stored (read) or to
be stored (write). The information about the _shards_ is shared across the
_Coordinators_ using the _Agency_.

ArangoDB organizes its collection data in _shards_. Sharding
allows to use multiple machines to run a cluster of ArangoDB
instances that together constitute a single database. This enables
you to store much more data, since ArangoDB distributes the data
automatically to the different servers. In many situations one can
also reap a benefit in data throughput, again because the load can
be distributed to multiple machines.

_Shards_ are configured per _collection_ so multiple _shards_ of data form
the _collection_ as a whole. To determine in which _shard_ the data is to
be stored ArangoDB performs a hash across the values. By default this
hash is being created from the document __key_.

For further information, please refer to the
[_Cluster Administration_ ](../../../Administration/Cluster/README.md#sharding) section.

Synchronous replication
-----------------------

In an ArangoDB Cluster, the replication among the data stored by the _DBServers_
is synchronous.

Synchronous replication works on a per-shard basis. Using the option _replicationFactor_,
one configures for each _collection_ how many copies of each _shard_ are kept in the Cluster.

{% hint 'danger' %}
If a collection has a _replication factor_ of 1, its data is **not**
replicated to other _DBServers_. This exposes you to a risk of data loss, if
the machine running the _DBServer_ with the only copy of the data fails permanently.

The _replication factor_ has to be set to a value equals or higher than 2
to achieve minimal data redundancy via the synchronous replication.

An equal-or-higher-than 2 _replication factor_ has to be set **explicitly**
when the collection is created, or can be set later at run time if you forgot
to set it at creation time.

When using a Cluster, please make sure all the collections that are important
(and should not be lost in any case) have a _replication factor_ equal or higher
than 2.
{% endhint %}

At any given time, one of the copies is declared to be the _leader_ and
all other replicas are _followers_. Internally, write operations for this _shard_
are always sent to the _DBServer_ which happens to hold the _leader_ copy,
which in turn replicates the changes to all _followers_ before the operation
is considered to be done and reported back to the _Coordinator_.
Internally, read operations are all served by the _DBServer_ holding the _leader_ copy,
this allows to provide snapshot semantics for complex transactions.

Using synchronous replication alone will guarantee consistency and high availability
at the cost of reduced performance: write requests will have a higher latency
(due to every write-request having to be executed on the _followers_) and
read requests will not scale out as only the _leader_ is being asked.

In a Cluster, synchronous replication will be managed by the _Coordinators_ for the client. 
The data will always be stored on the _DBServers_.

The following example will give you an idea of how synchronous operation
has been implemented in ArangoDB Cluster:

1. Connect to a _Coordinator_ via [_arangosh_](../../../Programs/Arangosh/README.md)
2. Create a collection

    127.0.0.1:8530@_system> db._create("test", {"replicationFactor": 2})

3. The _Coordinator_ will figure out a *leader* and one *follower* and create
   one *shard* (as this is the default)
4. Insert data

    127.0.0.1:8530@_system> db.test.insert({"foo": "bar"})

5. The _Coordinator_ will write the data to the _leader_, which in turn will
replicate it to the _follower_.
6. Only when both were successful the result is reported to be successful:

   ```json
   {
       "_id" : "test/7987",
       "_key" : "7987",
       "_rev" : "7987"
   }
   ```

Obviously, synchronous replication comes at the cost of an increased latency for
write operations, simply because there is one more network hop within the
Cluster for every request. Therefore the user can set the _replicationFactor_
to 1, which means that only one copy of each shard is kept, thereby
switching off synchronous replication. This is a suitable setting for
less important or easily recoverable data for which low latency write
operations matter.

Automatic failover
------------------

### Failure of a follower

If a _DBServer_ that holds a _follower_ copy of a _shard_ fails, then the _leader_
can no longer synchronize its changes to that _follower_. After a short timeout
(3 seconds), the _leader_ gives up on the _follower_ and declares it to be
out of sync.

One of the following two cases can happen:

**a)** If another _DBServer_ (that does not hold a _replica_ for this _shard_ already)
       is available in the Cluster, a new _follower_ will automatically
       be created on this other _DBServer_ (so the _replication factor_ constraint is
       satisfied again).

**b)** If no other _DBServer_ (that does not hold a _replica_ for this _shard_ already)
       is available, the service continues with one _follower_ less than the number
       prescribed by the _replication factor_.

If the old _DBServer_ with the _follower_ copy comes back, one of the following
two cases can happen:

**a)** If previously we were in case a), the _DBServer_ recognizes that there is a new
      _follower_ that was elected in the meantime, so it will no longer be a _follower_
       for that _shard_.

**b)** If previously we were in case b), the _DBServer_ automatically resynchronizes its
       data with the _leader_. The _replication factor_ constraint is now satisfied again
       and order is restored.

### Failure of a leader

If a _DBServer_ that holds a _leader_ copy of a shard fails, then the _leader_
can no longer serve any requests. It will no longer send a heartbeat to
the _Agency_. Therefore, a _supervision_ process running in the _Raft_ _leader_
of the Agency, can take the necessary action (after 15 seconds of missing
heartbeats), namely to promote one of the _DBServers_ that hold in-sync
replicas of the _shard_ to _leader_ for that _shard_. This involves a
reconfiguration in the _Agency_ and leads to the fact that _Coordinators_
now contact a different _DBServer_ for requests to this _shard_. Service
resumes. The other surviving _replicas_ automatically resynchronize their
data with the new _leader_. 

In addition to the above, one of the following two cases cases can happen:

a) If another _DBServer_ (that does not hold a _replica_ for this _shard_ already)
   is available in the Cluster, a new _follower_ will automatically
   be created on this other _DBServer_ (so the _replication factor_ constraint is
   satisfied again).
b) If no other _DBServer_ (that does not hold a _replica_ for this _shard_ already)
   is available the service continues with one _follower_ less than the number
   prescribed by the _replication factor_.

When the _DBServer_ with the original _leader_ copy comes back, it recognizes
that a new _leader_ was elected in the meantime, and one of the following
two cases can happen:

a) If previously we were in case a), since also a new _follower_ was created and
   the _replication factor_ constraint is satisfied, the _DBServer_ will no
   longer be a _follower_ for that _shard_.
b) If previously we were in case b), the _DBServer_ notices that it now holds
   a _follower_ _replica_ of that _shard_ and it resynchronizes its data with the
   new _leader_. The _replication factor_ constraint is now satisfied again, 
   and order is restored.

The following example will give you an idea of how _failover_
has been implemented in ArangoDB Cluster:

1. The _leader_ of a _shard_ (let's name it _DBServer001_) is going down.
2. A _Coordinator_ is asked to return a document:

    127.0.0.1:8530@_system> db.test.document("100069")

3. The _Coordinator_ determines which server is responsible for this document
   and finds _DBServer001_
4. The _Coordinator_ tries to contact _DBServer001_ and timeouts because it is
   not reachable.
5. After a short while the _supervision_ (running in parallel on the _Agency_)
   will see that _heartbeats_ from _DBServer001_ are not coming in
6. The _supervision_ promotes one of the _followers_ (say _DBServer002_), that
   is in sync, to be _leader_ and makes _DBServer001_ a _follower_.
7. As the _Coordinator_ continues trying to fetch the document it will see that
   the _leader_ changed to _DBServer002_
8. The _Coordinator_ tries to contact the new _leader_ (_DBServer002_) and returns
   the result:

    ```json
    {
        "_key" : "100069",
        "_id" : "test/100069",
        "_rev" : "513",
        "foo" : "bar"
    }
    ```
9. After a while the _supervision_ declares _DBServer001_ to be completely dead.
10. A new _follower_ is determined from the pool of _DBservers_.
11. The new _follower_ syncs its data from the _leader_ and order is restored.

Please note that there may still be timeouts. Depending on when exactly
the request has been done (in regard to the _supervision_) and depending
on the time needed to reconfigure the Cluster the _Coordinator_ might fail
with a timeout error.

Shard movement and resynchronization
------------------------------------

All _shard_ data synchronizations are done in an incremental way, such that
resynchronizations are quick. This technology allows to move shards
(_follower_ and _leader_ ones) between _DBServers_ without service interruptions.
Therefore, an ArangoDB Cluster can move all the data on a specific _DBServer_
to other _DBServers_ and then shut down that server in a controlled way.
This allows to scale down an ArangoDB Cluster without service interruption,
loss of fault tolerance or data loss. Furthermore, one can re-balance the
distribution of the _shards_, either manually or automatically.

All these operations can be triggered via a REST/JSON API or via the
graphical web UI. All fail-over operations are completely handled within
the ArangoDB Cluster.

Microservices and zero administation
------------------------------------

The design and capabilities of ArangoDB are geared towards usage in
modern microservice architectures of applications. With the
[Foxx services](../../../Foxx/README.md) it is very easy to deploy a data
centric microservice within an ArangoDB Cluster.

In addition, one can deploy multiple instances of ArangoDB within the
same project. One part of the project might need a scalable document
store, another might need a graph database, and yet another might need
the full power of a multi-model database actually mixing the various
data models. There are enormous efficiency benefits to be reaped by
being able to use a single technology for various roles in a project.

To simplify life of the _devops_ in such a scenario we try as much as
possible to use a _zero administration_ approach for ArangoDB. A running
ArangoDB Cluster is resilient against failures and essentially repairs
itself in case of temporary failures. 

Deployment
----------

An ArangoDB Cluster can be deployed in several ways, e.g. by manually
starting all the needed instances, by using the tool 
[_Starter_](../../../Programs/Starter/README.md), in
Docker, in Mesos or DC/OS, and in Kubernetes.

See the [Cluster Deployment](../../../Deployment/Cluster/README.md)
chapter for instructions.
