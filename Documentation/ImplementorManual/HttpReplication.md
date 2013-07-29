HTTP Interface for Replication {#HttpReplication}
=================================================

@NAVIGATE_HttpReplication
@EMBEDTOC{HttpReplicationTOC}

Replication {#HttpReplicationIntro}
===================================

This is an introduction to ArangoDB's HTTP replication interface.

The HTTP replication interface serves four main purposes:
- fetch initial data from a server (e.g. for an initial synchronisation of data, or backups)
- administer the replication logger (starting, stopping, querying state)
- fetch the changelog from a server (used for incremental synchronisation of changes)
- administer the replication applier (starting, stopping, configuring, querying state)

Replication Dump Commands {#HttpReplicationDumpCommands}
--------------------------------------------------------

The `inventory` method provides can be used to query an ArangoDB server's current
set of collections plus their indexes. Clients can use this method to get an 
overview of which collections are present on the server. They can use this information
to either start a full or a partial synchronisation of data, e.g. to initiate a backup
or the incremental data synchronisation.

@anchor HttpReplicationInventory
@copydetails triagens::arango::RestReplicationHandler::handleCommandInventory

The `dump` method can be used to fetch data from a specific collection. As the
results of the dump command can be huge, it may not return all data from a collection
at once. Instead, the dump command may be called repeatedly by replication clients
until there is no more data to fetch. The dump command will not only return the
current documents in the collection, but also document updates and deletions. 

To get to an identical state of data, replication clients should apply the individual
parts of the dump results in the same order as they are served to them.

@anchor HttpReplicationDump
@copydetails triagens::arango::RestReplicationHandler::handleCommandDump


Replication Logger Commands {#HttpReplicationLoggerCommands}
------------------------------------------------------------

The logger commands allow starting, starting, and fetching the current state of 
the replication logger. 

@anchor HttpReplicationLoggerStart
@copydetails triagens::arango::RestReplicationHandler::handleCommandLoggerStart

@CLEARPAGE
@anchor HttpReplicationLoggerStop
@copydetails triagens::arango::RestReplicationHandler::handleCommandLoggerStop

@CLEARPAGE
@anchor HttpReplicationLoggerState
@copydetails triagens::arango::RestReplicationHandler::handleCommandLoggerState

To query the latest changes logged by the replication logger, the Http interface
also provides the `logger-follow`.

This method should be used by replication clients to incrementally fetch updates 
from an ArangoDB instance.

@anchor HttpReplicationLoggerFollow
@copydetails triagens::arango::RestReplicationHandler::handleCommandLoggerFollow

Replication Applier Commands {#HttpReplicationApplierCommands}
--------------------------------------------------------------

The applier commands allow to remotely start, stop, and query the state and 
configuration of an ArangoDB server's replication applier.

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
