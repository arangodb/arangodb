
@startDocuBlock put_api_simple_first_example
@brief returns one document of a collection matching a given example

@RESTHEADER{PUT /_api/simple/first-example, Find documents matching an example}

@HINTS
{% hint 'warning' %}
This route should no longer be used.
All endpoints for Simple Queries are deprecated from version 3.4.0 on.
They are superseded by AQL queries.
{% endhint %}

{% hint 'warning' %}
Till ArangoDB versions 3.2.13 and 3.3.7 this API is quite expensive.
A more lightweight alternative is to use the HTTP Cursor API.
Starting from versions 3.2.14 and 3.3.8 this performance impact is not
an issue anymore, as the internal implementation of the API has changed.
{% endhint %}

@RESTBODYPARAM{collection,string,required,string}
The name of the collection to query.

@RESTBODYPARAM{example,string,required,string}
The example document.

@RESTDESCRIPTION

This will return the first document matching a given example.

Returns a result containing the document or *HTTP 404* if no
document matched the example.

If more than one document in the collection matches the specified example, only
one of these documents will be returned, and it is undefined which of the matching
documents is returned.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned when the query was successfully executed.

@RESTRETURNCODE{400}
is returned if the body does not contain a valid JSON representation of a
query. The response body contains an error document in this case.

@RESTRETURNCODE{404}
is returned if the collection specified by *collection* is unknown.  The
response body contains an error document in this case.

@EXAMPLES

If a matching document was found

@EXAMPLE_ARANGOSH_RUN{RestSimpleFirstExample}
    var cn = "products";
    db._drop(cn);
    var products = db._create(cn);
    products.save({ "a": { "k": 1, "j": 1 }, "i": 1});
    products.save({ "a": { "j": 1 }, "i": 1});
    products.save({ "i": 1});
    products.save({ "a": { "k": 2, "j": 2 }, "i": 1});
    var url = "/_api/simple/first-example";
    var body = { "collection": "products", "example" :  { "i" : 1 }  };

    var response = logCurlRequest('PUT', url, body);

    assert(response.code === 200);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

If no document was found

@EXAMPLE_ARANGOSH_RUN{RestSimpleFirstExampleNotFound}
    var cn = "products";
    db._drop(cn);
    var products = db._create(cn);
    products.save({ "a": { "k": 1, "j": 1 }, "i": 1});
    products.save({ "a": { "j": 1 }, "i": 1});
    products.save({ "i": 1});
    products.save({ "a": { "k": 2, "j": 2 }, "i": 1});
    var url = "/_api/simple/first-example";
    var body = { "collection": "products", "example" :  { "l" : 1 }  };

    var response = logCurlRequest('PUT', url, body);

    assert(response.code === 404);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
