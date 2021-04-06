
@startDocuBlock put_api_simple_all
@brief returns all documents of a collection

@RESTHEADER{PUT /_api/simple/all, Return all documents}

@HINTS
{% hint 'warning' %}
This route should no longer be used.
All endpoints for Simple Queries are deprecated from version 3.4.0 on.
They are superseded by AQL queries.
{% endhint %}

@RESTALLBODYPARAM{query,string,required}
Contains the query.

@RESTDESCRIPTION
Returns all documents of a collections. Equivalent to the AQL query
`FOR doc IN collection RETURN doc`. The call expects a JSON object
as body with the following attributes:

- *collection*: The name of the collection to query.

- *skip*: The number of documents to skip in the query (optional).

- *limit*: The maximal amount of documents to return. The *skip*
  is applied before the *limit* restriction (optional).

- *batchSize*: The number of documents to return in one go. (optional)

- *ttl*: The time-to-live for the cursor (in seconds, optional).

- *stream*: Create this cursor as a stream query (optional).

Returns a cursor containing the result.

@RESTRETURNCODES

@RESTRETURNCODE{201}
is returned if the query was executed successfully.

@RESTRETURNCODE{400}
is returned if the body does not contain a valid JSON representation of a
query. The response body contains an error document in this case.

@RESTRETURNCODE{404}
is returned if the collection specified by *collection* is unknown.  The
response body contains an error document in this case.

@EXAMPLES

Limit the amount of documents using *limit*

@EXAMPLE_ARANGOSH_RUN{RestSimpleAllSkipLimit}
    var cn = "products";
    db._drop(cn);
    var collection = db._create(cn);
    collection.save({"Hello1" : "World1" });
    collection.save({"Hello2" : "World2" });
    collection.save({"Hello3" : "World3" });
    collection.save({"Hello4" : "World4" });
    collection.save({"Hello5" : "World5" });

    var url = "/_api/simple/all";
    var body = '{ "collection": "products", "skip": 2, "limit" : 2 }';

    var response = logCurlRequest('PUT', url, body);

    assert(response.code === 201);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Using a *batchSize* value

@EXAMPLE_ARANGOSH_RUN{RestSimpleAllBatch}
    var cn = "products";
    db._drop(cn);
    var collection = db._create(cn);
    collection.save({"Hello1" : "World1" });
    collection.save({"Hello2" : "World2" });
    collection.save({"Hello3" : "World3" });
    collection.save({"Hello4" : "World4" });
    collection.save({"Hello5" : "World5" });

    var url = "/_api/simple/all";
    var body = '{ "collection": "products", "batchSize" : 3 }';

    var response = logCurlRequest('PUT', url, body);

    assert(response.code === 201);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
