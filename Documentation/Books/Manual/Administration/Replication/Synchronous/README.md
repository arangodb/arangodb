Synchronous Replication
=======================

At its core synchronous replication will replicate write operations to multiple hosts. This feature is only available when operating ArangoDB in a cluster. Whenever a coordinator executes a sychronously replicated write operation it will only be reported to be successful if it was carried out on all replicas. In contrast to multi master replication setups known from other systems ArangoDB's synchronous operation guarantees a consistent state across the cluster.
