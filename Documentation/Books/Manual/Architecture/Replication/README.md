Replication
===========

Replication allows you to *replicate* data onto another machine. It
forms the base of all disaster recovery and failover features ArangoDB
offers. 

ArangoDB offers **synchronous** and **asynchronous** replication.

Synchronous replication is used between the _DBServers_ of an ArangoDB
Cluster.

Asynchronous replication is used:

- between the _master_ and the _slave_ of an ArangoDB [_Master/Slave_](../../Architecture/DeploymentModes/MasterSlave/README.md) setup
- between the _Leader_ and the _Follower_ of an ArangoDB [_Active Failover_](../../Architecture/DeploymentModes/ActiveFailover/README.md) setup
- between multiple ArangoDB [Data Centers](../../Architecture/DeploymentModes/DC2DC/README.md) (inside the same Data Center replication is synchronous)

Synchronous replication
-----------------------

Synchronous replication only works within an ArangoDB Cluster and is typically
used for mission critical data which must be accessible at all
times. Synchronous replication generally stores a copy of a shard's
data on another DBServer and keeps it in sync. Essentially, when storing
data after enabling synchronous replication the Cluster will wait for
all replicas to write all the data before greenlighting the write
operation to the client. This will naturally increase the latency a
bit, since one more network hop is needed for each write. However, it
will enable the cluster to immediately fail over to a replica whenever
an outage has been detected, without losing any committed data, and
mostly without even signaling an error condition to the client. 

Synchronous replication is organized such that every _shard_ has a
_leader_ and `r-1` _followers_, where `r` denoted the replication
factor. The number of _followers_ can be controlled using the
`replicationFactor` parameter whenever you create a _collection_, the
`replicationFactor` parameter is the total number of copies being
kept, that is, it is one plus the number of _followers_. 

In addition to the `replicationFactor` we have a `minReplicationFactor`
the locks down a collection as soon as we have lost too many followers.


Asynchronous replication
------------------------

In ArangoDB any write operation is logged in the _write-ahead
log_. 

When using asynchronous replication _slaves_ (or _followers_)  
connect to a _master_ (or _leader_) and apply locally all the events from
the master log in the same order. As a result the _slaves_ (_followers_) 
will have the same state of data as the _master_ (_leader_).

_Slaves_ (_followers_) are only eventually consistent with the _master_ (_leader_).

Transactions are honored in replication, i.e. transactional write operations will 
become visible on _slaves_ atomically.

As all write operations will be logged to a master database's _write-ahead log_, the 
replication in ArangoDB currently cannot be used for write-scaling. The main purposes 
of the replication in current ArangoDB are to provide read-scalability and "hot backups" 
of specific databases.

It is possible to connect multiple _slave_ to the same _master_. _Slaves_ should be used
as read-only instances, and no user-initiated write operations 
should be carried out on them. Otherwise data conflicts may occur that cannot be solved 
automatically, and that will make the replication stop.

In an asynchronous replication scenario slaves will _pull_ changes 
from the _master_. _Slaves_ need to know to which _master_ they should 
connect to, but a _master_ is not aware of the _slaves_ that replicate from it. 
When the network connection between the _master_ and a _slave_ goes down, write 
operations on the master can continue normally. When the network is up again, _slaves_ 
can reconnect to the _master_ and transfer the remaining changes. This will 
happen automatically provided _slaves_ are configured appropriately.

Before 3.3.0 asynchronous replication was per database. Starting with 3.3.0 it is possible
to setup global replication.

### Replication lag

As decribed above, write operations are applied first in the _master_, and then applied 
in the _slaves_. 

For example, let's assume a write operation is executed in the _master_ 
at point in time _t0_. To make a _slave_ apply the same operation, it must first 
fetch the write operation's data from master's write-ahead log, then parse it and 
apply it locally. This will happen at some point in time after _t0_, let's say _t1_. 

The difference between _t1_ and _t0_ is called the _replication lag_, and it is unavoidable 
in asynchronous replication. The amount of replication _lag_ depends on many factors, a 
few of which are:

* the network capacity between the _slaves_ and the _master_
* the load of the _master_ and the _slaves_
* the frequency in which _slaves_ poll the _master_ for updates

Between _t0_ and _t1_, the state of data on the _master_ is newer than the state of data
on the _slaves_. At point in time _t1_, the state of data on the _master_ and _slaves_
is consistent again (provided no new data modifications happened on the _master_ in
between). Thus, the replication will lead to an _eventually consistent_ state of data.

### Replication overhead

As the _master_ servers are logging any write operation in the _write-ahead-log_ anyway replication doesn't cause any extra overhead on the _master_. However it will of course cause some overhead for the _master_ to serve incoming read requests of the _slaves_. Returning the requested data is however a trivial task for the _master_ and should not result in a notable performance degration in production.
