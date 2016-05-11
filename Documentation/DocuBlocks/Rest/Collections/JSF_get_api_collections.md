
@startDocuBlock JSF_get_api_collections
@brief returns all collections

@RESTHEADER{GET /_api/collection,reads all collections}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{excludeSystem,boolean,optional}
Whether or not system collections should be excluded from the result.

@RESTDESCRIPTION
Returns an object with an attribute *collections* containing an
array of all collection descriptions. The same information is also
available in the *names* as an object with the collection names
as keys.

By providing the optional query parameter *excludeSystem* with a value of
*true*, all system collections will be excluded from the response.

@RESTRETURNCODES

@RESTRETURNCODE{200}
The list of collections

@EXAMPLES

Return information about all collections:

@EXAMPLE_ARANGOSH_RUN{RestCollectionGetAllCollections}
    var url = "/_api/collection";

    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock

