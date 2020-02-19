
@startDocuBlock PostApiQueryProperties
@brief parse an AQL query and return information about it

@RESTHEADER{POST /_api/query, Parse an AQL query, parseQuery}

@RESTDESCRIPTION
This endpoint is for query validation only. To actually query the database,
see `/api/cursor`.

@RESTBODYPARAM{query,string,required,string}
To validate a query string without executing it, the query string can be
passed to the server via an HTTP POST request.

@RESTRETURNCODES

@RESTRETURNCODE{200}
If the query is valid, the server will respond with *HTTP 200* and
return the names of the bind parameters it found in the query (if any) in
the *bindVars* attribute of the response. It will also return an array
of the collections used in the query in the *collections* attribute.
If a query can be parsed successfully, the *ast* attribute of the returned
JSON will contain the abstract syntax tree representation of the query.
The format of the *ast* is subject to change in future versions of
ArangoDB, but it can be used to inspect how ArangoDB interprets a given
query. Note that the abstract syntax tree will be returned without any
optimizations applied to it.

@RESTRETURNCODE{400}
The server will respond with *HTTP 400* in case of a malformed request,
or if the query contains a parse error. The body of the response will
contain the error details embedded in a JSON object.

@EXAMPLES

a valid query

    @EXAMPLE_ARANGOSH_RUN{RestQueryValid}
    var url = "/_api/query";
    var body = '{ "query" : "FOR i IN 1..100 FILTER i > 10 LIMIT 2 RETURN i * 3" }';

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 200);

    logJsonResponse(response);
    @END_EXAMPLE_ARANGOSH_RUN

an invalid query

    @EXAMPLE_ARANGOSH_RUN{RestQueryInvalid}
    var url = "/_api/query";
    var body = '{ "query" : "FOR i IN 1..100 FILTER i = 1 LIMIT 2 RETURN i * 3" }';

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 400);

    logJsonResponse(response);
    @END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
