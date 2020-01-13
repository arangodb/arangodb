
@startDocuBlock get_api_replication_logger_return_state
@brief returns the state of the replication logger

@RESTHEADER{GET /_api/replication/logger-state, Return replication logger state, handleCommandLoggerState}

@RESTDESCRIPTION
Returns the current state of the server's replication logger. The state will
include information about whether the logger is running and about the last
logged tick value. This tick value is important for incremental fetching of
data.

The body of the response contains a JSON object with the following
attributes:

- *state*: the current logger state as a JSON object with the following
  sub-attributes:

  - *running*: whether or not the logger is running

  - *lastLogTick*: the tick value of the latest tick the logger has logged.
    This value can be used for incremental fetching of log data.

  - *totalEvents*: total number of events logged since the server was started.
    The value is not reset between multiple stops and re-starts of the logger.

  - *time*: the current date and time on the logger server

- *server*: a JSON object with the following sub-attributes:

  - *version*: the logger server's version

  - *serverId*: the logger server's id

- *clients*: returns the last fetch status by replication clients connected to
  the logger. Each client is returned as a JSON object with the following attributes:

  - *syncerId*: id of the client syncer

  - *serverId*: server id of client

  - *lastServedTick*: last tick value served to this client via the *logger-follow* API

  - *time*: date and time when this client last called the *logger-follow* API

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the logger state could be determined successfully.

@RESTRETURNCODE{405}
is returned when an invalid HTTP method is used.

@RESTRETURNCODE{500}
is returned if the logger state could not be determined.

@EXAMPLES

Returns the state of the replication logger.

@EXAMPLE_ARANGOSH_RUN{RestReplicationLoggerStateActive}
    var re = require("@arangodb/replication");

    var url = "/_api/replication/logger-state";

    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
