
@startDocuBlock JSF_get_api_return
@brief returns the server version number

@RESTHEADER{GET /_api/version, Return server version}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{details,boolean,optional}
If set to *true*, the response will contain a *details* attribute with
additional information about included components and their versions. The
attribute names and internals of the *details* object may vary depending on
platform and ArangoDB version.

@RESTDESCRIPTION
Returns the server name and version number. The response is a JSON object
with the following attributes:

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned in all cases.

@RESTREPLYBODY{server,string,required,string}
will always contain *arango*

@RESTREPLYBODY{version,string,required,string}
the server version string. The string has the format
"*major*.*minor*.*sub*". *major* and *minor* will be numeric, and *sub*
may contain a number or a textual version.

@RESTREPLYBODY{details,object,optional,}
an optional JSON object with additional details. This is
returned only if the *details* query parameter is set to *true* in the
request.

@EXAMPLES

Return the version information

@EXAMPLE_ARANGOSH_RUN{RestVersion}
    var response = logCurlRequest('GET', '/_api/version');

    assert(response.code === 200);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Return the version information with details

@EXAMPLE_ARANGOSH_RUN{RestVersionDetails}
    var response = logCurlRequest('GET', '/_api/version?details=true');

    assert(response.code === 200);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock

