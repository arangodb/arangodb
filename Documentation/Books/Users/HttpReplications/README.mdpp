!CHAPTER HTTP Interface for Replication 

!SUBSECTION Replication

This is an introduction to ArangoDB's HTTP replication interface.
The replication architecture and components are described in more details in 
[Replication](../Replication/README.md).

The HTTP replication interface serves four main purposes:
- fetch initial data from a server (e.g. for a backup, or for the initial synchronization 
  of data before starting the continuous replication applier)
- administer the replication logger (starting, stopping, querying state)
- fetch the changelog from a server (used for incremental synchronization of changes)
- administer the replication applier (starting, stopping, configuring, querying state)

Please note that all replication operations work on a database level. If an 
ArangoDB server contains more than one database, the replication system must be
configured individually per database, and replicating the data of multiple
databases will require multiple operations.

