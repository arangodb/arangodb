
@startDocuBlock get_api_wal_access_last_tick
@brief Return last available tick value

@RESTHEADER{GET /_api/wal/lastTick, Return last available tick value, handleCommandLastTick}

@RESTDESCRIPTION
Returns the last available tick value that can be served from the server's
replication log. This corresponds to the tick of the latest successfull operation.

The result is a JSON object containing the attributes *tick*, *time* and *server*.
* *tick*: contains the last available tick, *time*
* *time*: the server time as string in format "YYYY-MM-DDTHH:MM:SSZ"
* *server*: An object with fields *version* and *serverId*

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

@EXAMPLE_ARANGOSH_RUN{RestWalAccessFirstTick}
    var url = "/_api/wal/lastTick";
    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
