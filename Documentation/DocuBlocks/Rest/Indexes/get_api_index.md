
@startDocuBlock get_api_index
@brief returns all indexes of a collection

@RESTHEADER{GET /_api/index, Read all indexes of a collection, getIndexes}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{collection,string,required}
The collection name.

@RESTDESCRIPTION
Returns an object with an attribute *indexes* containing an array of all
index descriptions for the given collection. The same information is also
available in the *identifiers* as an object with the index handles as
keys.

@RESTRETURNCODES

@RESTRETURNCODE{200}
returns a JSON object containing a list of indexes on that collection.

@EXAMPLES

Return information about all indexes

@EXAMPLE_ARANGOSH_RUN{RestIndexAllIndexes}
    var cn = "products";
    db._drop(cn);
    db._create(cn);
    db[cn].ensureHashIndex("name");
    db[cn].ensureSkiplist("price", { sparse: true });

    var url = "/_api/index?collection=" + cn;

    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
