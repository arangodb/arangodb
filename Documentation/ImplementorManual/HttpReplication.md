HTTP Interface for Replication {#HttpReplication}
=================================================

@NAVIGATE_HttpReplication
@EMBEDTOC{HttpReplicationTOC}

Replication {#HttpReplicationIntro}
===================================

This is an introduction to ArangoDB's HTTP replication interface.
The replication architecture and components are described in more details in 
@ref UserManualReplication.

The HTTP replication interface serves four main purposes:
- fetch initial data from a server (e.g. for a backup, or for the initial synchronisation 
  of data before starting the continuous replication applier)
- administer the replication logger (starting, stopping, querying state)
- fetch the changelog from a server (used for incremental synchronisation of changes)
- administer the replication applier (starting, stopping, configuring, querying state)

Please note that all replication operations work on a database level. If an 
ArangoDB server contains more than one database, the replication system must be
configured individually per database, and replicating the data of multiple
databases will require multiple operations.

Replication Dump Commands {#HttpReplicationDumpCommands}
--------------------------------------------------------

The `inventory` method can be used to query an ArangoDB database's current
set of collections plus their indexes. Clients can use this method to get an 
overview of which collections are present in the database. They can use this information
to either start a full or a partial synchronisation of data, e.g. to initiate a backup
or the incremental data synchronisation.

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


Replication Logger Commands {#HttpReplicationLoggerCommands}
------------------------------------------------------------

The logger commands allow starting, starting, and fetching the current state of a
database's replication logger. 

@anchor HttpReplicationLoggerGetConfig
@copydetails triagens::arango::RestReplicationHandler::handleCommandLoggerGetConfig

@CLEARPAGE
@anchor HttpReplicationLoggerSetConfig
@copydetails triagens::arango::RestReplicationHandler::handleCommandLoggerSetConfig

@CLEARPAGE
@anchor HttpReplicationLoggerStart
@copydetails triagens::arango::RestReplicationHandler::handleCommandLoggerStart

@CLEARPAGE
@anchor HttpReplicationLoggerStop
@copydetails triagens::arango::RestReplicationHandler::handleCommandLoggerStop

@CLEARPAGE
@anchor HttpReplicationLoggerState
@copydetails triagens::arango::RestReplicationHandler::handleCommandLoggerState

To query the latest changes logged by the replication logger, the HTTP interface
also provides the `logger-follow`.

This method should be used by replication clients to incrementally fetch updates 
from an ArangoDB database.

@anchor HttpReplicationLoggerFollow
@copydetails triagens::arango::RestReplicationHandler::handleCommandLoggerFollow

Replication Applier Commands {#HttpReplicationApplierCommands}
--------------------------------------------------------------

The applier commands allow to remotely start, stop, and query the state and 
configuration of an ArangoDB database's replication applier.

@anchor HttpReplicationApplierGetConfig
@copydetails triagens::arango::RestReplicationHandler::handleCommandApplierGetConfig

@CLEARPAGE
@anchor HttpReplicationApplierSetConfig
@copydetails triagens::arango::RestReplicationHandler::handleCommandApplierSetConfig

@CLEARPAGE
@anchor HttpReplicationApplierStart
@copydetails triagens::arango::RestReplicationHandler::handleCommandApplierStart

@CLEARPAGE
@anchor HttpReplicationApplierStop
@copydetails triagens::arango::RestReplicationHandler::handleCommandApplierStop

@CLEARPAGE
@anchor HttpReplicationApplierGetState
@copydetails triagens::arango::RestReplicationHandler::handleCommandApplierGetState

Other Replication Commands {#HttpReplicationOtherCommands}
----------------------------------------------------------

@anchor HttpReplicationServerId
@copydetails triagens::arango::RestReplicationHandler::handleCommandServerId
