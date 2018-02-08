Introduction to Replication
===========================

Replication allows you to *replicate* data onto another machine. It
forms the base of all disaster recovery and failover features ArangoDB
offers. 

ArangoDB offers **asynchronous** and **synchronous** replication,
depending on which type of arangodb deployment you are using.

Since ArangoDB 3.2 the *synchronous replication* replication is the *only* replication
type used in a cluster whereas the asynchronous replication is only available between
single-server nodes. Future versions of ArangoDB may reintroduce asynchronous
replication for the cluster.

### Asynchronous replication

In ArangoDB any write, delete or update operation will be logged to the write-ahead
log (WAL), before beeing acknowledged to the client. The write ahead ensures
that in the occurance of an unscheduled server shutdown, we are still
able to restore already commited transactions.

The asynchronous replication is used as the foundation of most replication operations in
arangodb. Effecitvely you as a potential end-user will **only** ever encouter it directly
**on single-instance**nodes.

The WAL is also useful for other purposes than data-integrety, it also allows
other replication clients to follow along with all changes made on a specific *leading* server.
These async-replication clients are called *followers*, which will periodically poll the *leader*
for changes to it's data. The clients will apply all modification operations in the same order
on the local node. Assuming the initial dataset was identical to the dataset on the leader node,
the follower nodes should eventually have the exact same state as the leader.

The asynchronous replication allows you to achieve **eventual consistency** between one leader
ArangoDB node and multiple follower nodes. For more information see the [relevant sections](Asynchronous/README.md)

### Synchronous replication

Synchronous replication is only available in the context of an ArangodB **cluster setup**.
In the case of synchronous replication we are always talking about replicating **shards** of a collection
(a shard is a fragment of a collection, stored on one DBServer node). Synchronous replication guarantees that
replicated copies of a *leader* shard are stored on another *follower* db server and kept completely **in sync**.

By "in-sync" we mean that we guarantees *strong consistency* between replica shards: Your data is
available in an identical version on all "in-sync" followers. This is in contrast to *eventual consistency*
where a follower server might contain slightly older data than the leader server. Synchronously replicated
shards are always identical to their leader shard and can take over in case of a server outage.

Synchronous replication can even work reliably for for mission critical data which must be accessible at all
times. . Essentially, when storing data after enabling synchronous replication the cluster will wait for
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

#### Satellite collections

Satellite collections are synchronously replicated collections having a dynamic replicationFactor.
They will replicate all data to all database servers allowing the database servers to join data
locally instead of doing heavy network operations.

**Satellite collections are an enterprise only feature.**

