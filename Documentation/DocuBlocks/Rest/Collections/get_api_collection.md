
@startDocuBlock get_api_collection

@RESTHEADER{GET /_api/collection, List all collections, listCollections}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{excludeSystem,boolean,optional}
Whether or not system collections should be excluded from the result.

@RESTDESCRIPTION
Returns an object with a `result` attribute containing an array with the
descriptions of all collections in the current database.

By providing the optional `excludeSystem` query parameter with a value of
`true`, all system collections are excluded from the response.

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
