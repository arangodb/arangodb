Replication
===========================

Replication allows you to *replicate* data onto another machine. It
forms the base of all disaster recovery and failover features ArangoDB
offers. 

ArangoDB offers **synchronous** and **asynchronous** replication.

Synchronous replication is used between the _DBServers_ of an ArangoDB
Cluster.

Asynchronous replication is used:
- when ArangoDB is operating in _Master/Slave_ or _Active Failover_ mode
- among multiple Data Centers

For more information on the ArangoDB Server _modes_ please refer to the
[_Server Modes_](../../Architecture/ServerModes.md) section.

### Synchronous replication

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

Synchronous replication is organized such that every shard has a
leader and `r-1` followers, where `r` denoted the replication
factor. The number of followers can be controlled using the
`replicationFactor` parameter whenever you create a collection, the
`replicationFactor` parameter is the total number of copies being
kept, that is, it is one plus the number of followers. 

### Asynchronous replication

In ArangoDB any write operation will be logged to the write-ahead
log. When using asynchronous replication slaves (or followers) will 
connect to a master (or leader) and apply all the events from the log
in the same order locally. After that, they will have the same state of data as the
master database.





