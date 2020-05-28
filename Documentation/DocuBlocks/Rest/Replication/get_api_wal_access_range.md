
@startDocuBlock get_api_wal_access_range
@brief returns the tick ranges available in the write-ahead-log

@RESTHEADER{GET /_api/wal/range, Return tick ranges available in the operations of WAL, handleCommandTickRange}

@RESTDESCRIPTION
Returns the currently available ranges of tick values for all WAL files.
The tick values can be used to determine if certain
data (identified by tick value) are still available for replication.

The body of the response contains a JSON object.
* *tickMin*: minimum tick available
* *tickMax*: maximum tick available
* *time*: the server time as string in format "YYYY-MM-DDTHH:MM:SSZ"
* *server*: An object with fields *version* and *serverId*

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the tick ranges could be determined successfully.

@RESTRETURNCODE{405}
is returned when an invalid HTTP method is used.

@RESTRETURNCODE{500}
is returned if the server operations state could not be determined.

@RESTRETURNCODE{501}
is returned when this operation is called on a Coordinator in a cluster.

@EXAMPLES

Returns the available tick ranges.

@EXAMPLE_ARANGOSH_RUN{RestWalAccessTickRange}
    var url = "/_api/wal/range";

    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
