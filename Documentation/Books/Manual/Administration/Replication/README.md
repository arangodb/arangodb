!CHAPTER Introduction to Replication

Replication allows you to *replicate* data onto another machine. It forms the base of all disaster recovery and failover features ArangoDB offers.

ArangoDB offers asynchronous and synchronous replication which both have their pros and cons. Both modes may and should be combined in a real world scenario and be applied in the usecase where they excel most.

We will describe pros and cons of each of them in the following sections.

!SUBSECTION Synchronous replication

Synchronous replication only works in in a cluster and is typically used for mission critical data which must be accessible at all times. Synchronous replication generally stores a copy of the data on another host and keeps it in sync. Essentially when storing data after enabling synchronous replication the cluster will wait for all replicas to write all the data before greenlighting the write operation to the client. This will naturally increase the latency a bit, since one more network hop is needed for each write. However it will enable the cluster to immediately fail over to a replica whenever an outage has been detected, without losing any committed data, and mostly without even signaling an error condition to the client.

Synchronous replication is organized in a way that every shard has a leader and r-1 followers. The number of followers can be controlled using the `replicationFactor` whenever you create a collection, the `replicationFactor` is the total number of copies being kept, that is, it is one plus the number of followers.

!SUBSECTION Asynchronous replication

In ArangoDB any write operation will be logged to the write-ahead log. When using Asynchronous replication slaves will connect to a master and apply all the events from the log in the same order locally. After that, they will have the same state of data as the master database.
