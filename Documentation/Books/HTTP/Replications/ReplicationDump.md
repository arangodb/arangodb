Replication Dump Commands
=========================

The *inventory* method can be used to query an ArangoDB database's current
set of collections plus their indexes. Clients can use this method to get an 
overview of which collections are present in the database. They can use this information
to either start a full or a partial synchronization of data, e.g. to initiate a backup
or the incremental data synchronization.

<!-- arangod/RestHandler/RestReplicationHandler.cpp -->
@startDocuBlock JSF_put_api_replication_inventory


The *batch* method will create a snapshot of the current state that then can be
dumped. A batchId is required when using the dump api with rocksdb.

@startDocuBlock JSF_post_batch_replication

@startDocuBlock JSF_delete_batch_replication

@startDocuBlock JSF_put_batch_replication


The *dump* method can be used to fetch data from a specific collection. As the
results of the dump command can be huge, *dump* may not return all data from a collection
at once. Instead, the dump command may be called repeatedly by replication clients
until there is no more data to fetch. The dump command will not only return the
current documents in the collection, but also document updates and deletions.

Please note that the *dump* method will only return documents, updates and deletions
from a collection's journals and datafiles. Operations that are stored in the write-ahead
log only will not be returned. In order to ensure that these operations are included
in a dump, the write-ahead log must be flushed first. 

To get to an identical state of data, replication clients should apply the individual
parts of the dump results in the same order as they are provided.

<!-- arangod/RestHandler/RestReplicationHandler.cpp -->
@startDocuBlock JSF_get_api_replication_dump


<!-- arangod/RestHandler/RestReplicationHandler.cpp -->
@startDocuBlock JSF_put_api_replication_synchronize

<!-- arangod/RestHandler/RestReplicationHandler.cpp -->
@startDocuBlock JSF_get_api_replication_cluster_inventory
