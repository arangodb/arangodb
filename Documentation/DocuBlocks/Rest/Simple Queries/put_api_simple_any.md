
@startDocuBlock put_api_simple_any
@brief returns a random document from a collection

@RESTHEADER{PUT /_api/simple/any, Return a random document}

@HINTS
{% hint 'warning' %}
This route should no longer be used.
All endpoints for Simple Queries are deprecated from version 3.4.0 on.
They are superseded by AQL queries.
{% endhint %}

@RESTDESCRIPTION
Returns a random document from a collection. The call expects a JSON object
as body with the following attributes:

@RESTBODYPARAM{collection,string,required, string}
The identifier or name of the collection to query.<br>
Returns a JSON object with the document stored in the attribute
*document* if the collection contains at least one document. If
the collection is empty, the *document* attribute contains null.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the query was executed successfully.

@RESTRETURNCODE{400}
is returned if the body does not contain a valid JSON representation of a
query. The response body contains an error document in this case.

@RESTRETURNCODE{404}
is returned if the collection specified by *collection* is unknown.  The
response body contains an error document in this case.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestSimpleAny}
    var cn = "products";
    db._drop(cn);
    var collection = db._create(cn);
    collection.save({"Hello1" : "World1" });
    collection.save({"Hello2" : "World2" });
    collection.save({"Hello3" : "World3" });
    collection.save({"Hello4" : "World4" });
    collection.save({"Hello5" : "World5" });

    var url = "/_api/simple/any";
    var body = { "collection": "products" };

    var response = logCurlRequest('PUT', url, body);

    assert(response.code === 200);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
