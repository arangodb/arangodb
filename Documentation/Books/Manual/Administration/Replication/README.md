Replication
===========================

Replication allows you to *replicate* data onto another machine. It
forms the base of all disaster recovery and failover features ArangoDB
offers. 

ArangoDB offers **synchronous** and **asynchronous** replication.

Synchronous replication is used between the _DBServers_ of an ArangoDB
Cluster.

Asynchronous replication is used:
- when ArangoDB is operating in _Master/Slave_ or _Active Failover_ modes
- among multiple Data Centers

For more information on the ArangoDB Server _modes_ please refer to the
[_Server Modes_](../../Architecture/ServerModes.md) section.

## Synchronous replication

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

## Asynchronous replication

In ArangoDB any write operation will be logged to the _write-ahead
log_. When using asynchronous replication _slaves_ (or _followers_) will 
connect to a _master_ (or _leader_) and apply all the events from the log
in the same order locally. After that, they will have the same state
of data as the _master_ database.

