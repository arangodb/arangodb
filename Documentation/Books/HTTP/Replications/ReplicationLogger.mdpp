Replication Logger Commands
===========================

Previous versions of ArangoDB allowed starting, stopping and configuring the
replication logger. These commands are superfluous in ArangoDB 2.2 as all
data-modification operations are written to the server's write-ahead log and are
not handled by a separate logger anymore.

The only useful operations remaining since ArangoDB 2.2 are to query the current state
of the logger and to fetch the latest changes written by the logger. The operations
will return the state and data from the write-ahead log.

<!-- arangod/RestHandler/RestReplicationHandler.cpp -->
@startDocuBlock JSF_get_api_replication_logger_return_state

To query the latest changes logged by the replication logger, the HTTP interface
also provides the `logger-follow` method.

This method should be used by replication clients to incrementally fetch updates 
from an ArangoDB database.

<!-- arangod/RestHandler/RestReplicationHandler.cpp -->
@startDocuBlock JSF_get_api_replication_logger_returns

To check what range of changes is available (identified by tick values), the HTTP
interface provides the methods `logger-first-tick` and `logger-tick-ranges`.
Replication clients can use the methods to determine if certain data (identified
by a tick *date*) is still available on the master.

@startDocuBlock JSF_get_api_replication_logger_first_tick

@startDocuBlock JSF_get_api_replication_logger_tick_ranges

