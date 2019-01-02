<!-- don't edit here, it's from https://@github.com/arangodb/arangosync.git / docs/Manual/ -->
# When to use it... and when not

The _datacenter to datacenter replication_ is a good solution in all cases where
you want to replicate data from one cluster to another without the requirement
that the data is available immediately in the other cluster.

The _datacenter to datacenter replication_ is not a good solution when one of the
following applies:

- You want to replicate data from cluster A to cluster B and from cluster B
  to cluster A at the same time.
- You need synchronous replication between 2 clusters.
- There is no network connection between cluster A and B.
- You want complete control over which database, collection & documents are replicate and which not.
