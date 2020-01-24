
@startDocuBlock get_api_replication_logger_first_tick
@brief Return the first available tick value from the server

@RESTHEADER{GET /_api/replication/logger-first-tick, Returns the first available tick value, handleCommandLoggerFirstTick}

@RESTDESCRIPTION
Returns the first available tick value that can be served from the server's
replication log. This method can be called by replication clients after to
determine if certain data (identified by a tick value) is still available
for replication.

The result is a JSON object containing the attribute *firstTick*. This
attribute contains the minimum tick value available in the server's
replication
log.

**Note**: this method is not supported on a Coordinator in a cluster.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the request was executed successfully.

@RESTRETURNCODE{405}
is returned when an invalid HTTP method is used.

@RESTRETURNCODE{500}
is returned if an error occurred while assembling the response.

@RESTRETURNCODE{501}
is returned when this operation is called on a Coordinator in a cluster.

@EXAMPLES

Returning the first available tick

@EXAMPLE_ARANGOSH_RUN{RestReplicationLoggerFirstTick}
    var url = "/_api/replication/logger-first-tick";
    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
