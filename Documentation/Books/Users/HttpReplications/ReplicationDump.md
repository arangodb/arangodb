!CHAPTER Replication Dump Commands

The `inventory` method can be used to query an ArangoDB database's current
set of collections plus their indexes. Clients can use this method to get an 
overview of which collections are present in the database. They can use this information
to either start a full or a partial synchronization of data, e.g. to initiate a backup
or the incremental data synchronization.

@anchor HttpReplicationInventory
@copydetails triagens::arango::RestReplicationHandler::handleCommandInventory

The `dump` method can be used to fetch data from a specific collection. As the
results of the dump command can be huge, `dump` may not return all data from a collection
at once. Instead, the dump command may be called repeatedly by replication clients
until there is no more data to fetch. The dump command will not only return the
current documents in the collection, but also document updates and deletions. 

To get to an identical state of data, replication clients should apply the individual
parts of the dump results in the same order as they are served to them.

@anchor HttpReplicationDump
@copydetails triagens::arango::RestReplicationHandler::handleCommandDump

The `sync` method can be used by replication clients to connect an ArangoDB database 
to a remote endpoint, fetch the remote list of collections and indexes, and collection
data. 
It will thus create a local backup of the state of data at the remote ArangoDB database.
`sync` works on a database level. 

`sync` will first fetch the list of collections and indexes from the remote endpoint.
It does so by calling the `inventory` API of the remote database. It will then purge
data in the local ArangoDB database, and after start will transfer collection data 
from the remote database to the local ArangoDB database. It will extract data from the
remote database by calling the remote database's `dump` API until all data are fetched.

As mentioned, `sync` will remove data from the local instance, and thus must be handled
with caution.

@anchor HttpReplicationSync
@copydetails triagens::arango::RestReplicationHandler::handleCommandSync


