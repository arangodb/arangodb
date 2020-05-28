
@startDocuBlock get_api_endpoint
@brief This API call returns the list of all endpoints (single server).

@RESTHEADER{GET /_api/endpoint, Return list of all endpoints, retrieveEndpoints}

@HINTS
{% hint 'warning' %}
This route should no longer be used.
It is considered as deprecated from version 3.4.0 on.
{% endhint %}

@RESTDESCRIPTION
Returns an array of all configured endpoints the server is listening on.

The result is a JSON array of JSON objects, each with `"entrypoint"' as
the only attribute, and with the value being a string describing the
endpoint.

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

    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
