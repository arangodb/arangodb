<a name="replication_logger_commands"></a>
# Replication Logger Commands

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

