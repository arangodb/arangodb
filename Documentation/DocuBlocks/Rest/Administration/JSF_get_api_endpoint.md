
@startDocuBlock JSF_get_api_endpoint
@brief returns a list of all endpoints

@RESTHEADER{GET /_api/endpoint, Return list of all endpoints}

@RESTDESCRIPTION
Returns an array of all configured endpoints the server is listening on. For
each endpoint, the array of allowed databases is returned too if set.

The result is a JSON object which has the endpoints as keys, and an array of
mapped database names as values for each endpoint.

If an array of mapped databases is empty, it means that all databases can be
accessed via the endpoint. If an array of mapped databases contains more than
one database name, this means that any of the databases might be accessed
via the endpoint, and the first database in the arry will be treated as
the default database for the endpoint. The default database will be used
when an incoming request does not specify a database name in the request
explicitly.

**Note**: retrieving the array of all endpoints is allowed in the system database
only. Calling this action in any other database will make the server return
an error.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned when the array of endpoints can be determined successfully.

@RESTRETURNCODE{400}
is returned if the action is not carried out in the system database.

@RESTRETURNCODE{405}
The server will respond with *HTTP 405* if an unsupported HTTP method is used.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestEndpointGet}
    var url = "/_api/endpoint";
    var endpoint = "tcp://127.0.0.1:8532";
    var body = {
      endpoint: endpoint,
      databases: [ "mydb1", "mydb2" ]
    };
    curlRequest('POST', url, JSON.stringify(body));

    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
    curlRequest('DELETE', url + '/' + encodeURIComponent(endpoint));
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock

