
@startDocuBlock JSF_get_api_views
@brief returns all views

@RESTHEADER{GET /_api/view,reads all views}

@RESTQUERYPARAMETERS

@RESTDESCRIPTION
Returns an object containing an array of all view descriptions.

@RESTRETURNCODES

@RESTRETURNCODE{200}
The list of views

@EXAMPLES

Return information about all views:

    var url = "/_api/view";

    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
@endDocuBlock

