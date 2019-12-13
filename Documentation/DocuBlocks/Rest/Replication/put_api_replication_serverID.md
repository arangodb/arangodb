
@startDocuBlock put_api_replication_serverID
@brief fetch this server's unique identifier

@RESTHEADER{GET /_api/replication/server-id, Return server id, handleCommandServerId}

@RESTDESCRIPTION
Returns the servers id. The id is also returned by other replication API
methods, and this method is an easy means of determining a server's id.

The body of the response is a JSON object with the attribute *serverId*. The
server id is returned as a string.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the request was executed successfully.

@RESTRETURNCODE{405}
is returned when an invalid HTTP method is used.

@RESTRETURNCODE{500}
is returned if an error occurred while assembling the response.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestReplicationServerId}
    var url = "/_api/replication/server-id";
    var response = logCurlRequest('GET', url);

    assert(response.code === 200);
    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
