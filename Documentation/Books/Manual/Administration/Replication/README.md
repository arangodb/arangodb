Introduction to Replication
===========================

Replication allows you to *replicate* data onto another machine. It
forms the base of all disaster recovery and failover features ArangoDB
offers. 

ArangoDB offers asynchronous and synchronous replication which both
have their pros and cons. Both modes may and should be combined in a
real world scenario and be applied in the usecase where they excel
most. 

We will describe pros and cons of each of them in the following
sections. 

### Synchronous replication

Synchronous replication only works within a cluster and is typically
used for mission critical data which must be accessible at all
times. Synchronous replication generally stores a copy of a shard's
data on another db server and keeps it in sync. Essentially, when storing
data after enabling synchronous replication the cluster will wait for
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

### Satellite collections

Satellite collections are synchronously replicated collections having a dynamic replicationFactor.
They will replicate all data to all database servers allowing the database servers to join data
locally instead of doing heavy network operations.

Satellite collections are an enterprise only feature.

### Asynchronous replication

In ArangoDB any write operation will be logged to the write-ahead
log. When using Asynchronous replication slaves will connect to a
master and apply all the events from the log in the same order
locally. After that, they will have the same state of data as the
master database. 
