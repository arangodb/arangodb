Cluster Architecture
====================

The cluster architecture of ArangoDB is a _CP_ master/master model with no 
single point of failure. With "CP" we mean that in the presence of a
network partition, the database prefers internal consistency over 
availability. With "master/master" we mean that clients can send their 
requests to an arbitrary node, and experience the same view on the
database regardless. "No single point of failure" means that the cluster
can continue to serve requests, even if one machine fails completely.

In this way, ArangoDB has been designed as a distributed multi-model 
database. This section gives a short outline on the cluster architecture and
how the above features and capabilities are achieved.

## Structure of an ArangoDB Cluster

An ArangoDB Cluster consists of a number of ArangoDB instances
which talk to each other over the network. They play different roles,
which will be explained in detail below. The current configuration 
of the Cluster is held in the _Agency_, which is a highly-available
resilient key/value store based on an odd number of ArangoDB instances
running [Raft Consensus Protocol](https://raft.github.io/).

For the various instances in an ArangoDB Cluster there are 3 distinct
roles:

- _Agents_
- _Coordinators_
- _DBServers_. 

In the following sections we will shed light on each of them. 

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

DBservers are the ones where the data is actually hosted. They
host shards of data and using synchronous replication a DBServer may
either be leader or follower for a shard.

They should not be accessed from the outside but indirectly through the
_Coordinators_. They may also execute queries in part or as a whole when
asked by a _Coordinator_.
 
## Cluster ID

Every non-Agency ArangoDB instance in a Cluster is assigned a unique
ID during its startup. Using its ID a node is identifiable
throughout the Cluster. All cluster operations will communicate
via this ID.

## Sharding

Using the roles outlined above an ArangoDB Cluster is able to distribute
data in so called _shards_ across multiple DBServers. From the outside
this process is fully transparent and as such we achieve the goals of
what other systems call "master-master replication". In an ArangoDB
Cluster you talk to any _Coordinator_ and whenever you read or write data
it will automatically figure out where the data is stored (read) or to
be stored (write). The information about the _shards_ is shared across the
_Coordinators_ using the _Agency_.

ArangoDB organizes its collection data in shards. Sharding
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
[_Cluster Administration_ ](../Administration/Cluster/README.md#sharding) section.

## Many sensible configurations

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

As you acn see, the _Coordinator_ layer can be scaled and deployed independently
from the _DBServer_ layer.

### Synchronous replication with automatic fail-over

In an ArangoDB Cluster, the replication among the data stored by the _DBServers_
is synchronous.

Synchronous replication works on a per-shard basis. One configures for
each collection, how many copies of each _shard_ are kept in the Cluster.
At any given time, one of the copies is declared to be the "leader" and
all other replicas are "followers". Write operations for this shard
are always sent to the _DBServer_ which happens to hold the leader copy,
which in turn replicates the changes to all followers before the operation
is considered to be done and reported back to the coordinator.
Read operations are all served by the server holding the leader copy,
this allows to provide snapshot semantics for complex transactions.

If a DBserver fails that holds a follower copy of a shard, then the leader
can no longer synchronize its changes to that follower. After a short timeout
(3 seconds), the leader gives up on the follower, declares it to be
out of sync, and continues service without the follower. When the server
with the follower copy comes back, it automatically resynchronizes its
data with the leader and synchronous replication is restored.

If a DBserver fails that holds a leader copy of a shard, then the leader
can no longer serve any requests. It will no longer send a heartbeat to 
the Agency. Therefore, a supervision process running in the Raft leader
of the Agency, can take the necessary action (after 15 seconds of missing
heartbeats), namely to promote one of the servers that hold in-sync
replicas of the shard to leader for that shard. This involves a 
reconfiguration in the Agency and leads to the fact that coordinators
now contact a different DBserver for requests to this shard. Service
resumes. The other surviving replicas automatically resynchronize their
data with the new leader. When the DBserver with the original leader
copy comes back, it notices that it now holds a follower replica,
resynchronizes its data with the new leader and order is restored.

All shard data synchronizations are done in an incremental way, such that
resynchronizations are quick. This technology allows to move shards
(follower and leader ones) between DBservers without service interruptions.
Therefore, an ArangoDB cluster can move all the data on a specific DBserver
to other DBservers and then shut down that server in a controlled way. 
This allows to scale down an ArangoDB cluster without service interruption,
loss of fault tolerance or data loss. Furthermore, one can re-balance the
distribution of the shards, either manually or automatically.

All these operations can be triggered via a REST/JSON API or via the
graphical web UI. All fail-over operations are completely handled within
the ArangoDB cluster.

Obviously, synchronous replication involves a certain increased latency for
write operations, simply because there is one more network hop within the 
cluster for every request. Therefore the user can set the replication factor
to 1, which means that only one copy of each shard is kept, thereby
switching off synchronous replication. This is a suitable setting for
less important or easily recoverable data for which low latency write 
operations matter.

## Microservices and zero administation

The design and capabilities of ArangoDB are geared towards usage in
modern microservice architectures of applications. With the 
[Foxx services](../Foxx/README.md) it is very easy to deploy a data
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
itself in case of temporary failures. See the next section for further
capabilities in this direction.

## Apache Mesos integration

For the distributed setup, we use the Apache Mesos infrastructure by default.

ArangoDB is a fully certified package for DC/OS and can thus
be deployed essentially with a few mouse clicks or a single command, once
you have an existing DC/OS cluster. But even on a plain Apache Mesos cluster
one can deploy ArangoDB via Marathon with a single API call and some JSON 
configuration.

The advantage of this approach is that we can not only implement the 
initial deployment, but also the later management of automatic 
replacement of failed instances and the scaling of the ArangoDB cluster
(triggered manually or even automatically). Since all manipulations are
either via the graphical web UI or via JSON/REST calls, one can even
implement auto-scaling very easily.

A DC/OS cluster is a very natural environment to deploy microservice
architectures, since it is so convenient to deploy various services,
including potentially multiple ArangoDB cluster instances within the
same DC/OS cluster. The built-in service discovery makes it extremely
simple to connect the various microservices and Mesos automatically
takes care of the distribution and deployment of the various tasks.

See the [Deployment](../Deployment/README.md) chapter and its subsections
for instructions.

It is possible to deploy an ArangoDB cluster by simply launching a bunch of 
Docker containers with the right command line options to link them up, 
or even on a single machine starting multiple ArangoDB processes. In that 
case, synchronous replication will work within the deployed ArangoDB cluster,
and automatic fail-over in the sense that the duties of a failed server will
automatically be assigned to another, surviving one. However, since the
ArangoDB cluster cannot within itself launch additional instances, replacement
of failed nodes is not automatic and scaling up and down has to be managed
manually. This is why we do not recommend this setup for production 
deployment.
