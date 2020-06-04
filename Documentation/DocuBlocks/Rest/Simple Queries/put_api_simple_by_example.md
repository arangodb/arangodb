
@startDocuBlock put_api_simple_by_example
@brief returns all documents of a collection matching a given example

@RESTHEADER{PUT /_api/simple/by-example, Simple query by-example}

@HINTS
{% hint 'warning' %}
This route should no longer be used.
All endpoints for Simple Queries are deprecated from version 3.4.0 on.
They are superseded by AQL queries.
{% endhint %}

{% hint 'warning' %}
Till ArangoDB versions 3.2.13 and 3.3.7 this API is quite expensive.
A more lightweight alternative is to use the [HTTP Cursor API](../AqlQueryCursor/README.md).
Starting from versions 3.2.14 and 3.3.8 this performance impact is not
an issue anymore, as the internal implementation of the API has changed.
{% endhint %}

@RESTBODYPARAM{collection,string,required,string}
The name of the collection to query.

@RESTBODYPARAM{example,string,required,string}
The example document.

@RESTBODYPARAM{skip,string,required,string}
The number of documents to skip in the query (optional).

@RESTBODYPARAM{limit,string,required,string}
The maximal amount of documents to return. The *skip*
is applied before the *limit* restriction. (optional)

@RESTBODYPARAM{batchSize,integer,optional,int64}
maximum number of result documents to be transferred from
the server to the client in one roundtrip. If this attribute is
not set, a server-controlled default value will be used. A *batchSize* value of
*0* is disallowed.

@RESTDESCRIPTION

This will find all documents matching a given example.

Returns a cursor containing the result, see [HTTP Cursor](../AqlQueryCursor/README.md) for details.

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

Matching an attribute

@EXAMPLE_ARANGOSH_RUN{RestSimpleByExample}
    var cn = "products";
    db._drop(cn);
    var products = db._create(cn);
    products.save({ "a": { "k": 1, "j": 1 }, "i": 1});
    products.save({ "a": { "j": 1 }, "i": 1});
    products.save({ "i": 1});
    products.save({ "a": { "k": 2, "j": 2 }, "i": 1});
    var url = "/_api/simple/by-example";
    var body = { "collection": "products", "example" :  { "i" : 1 }  };

    var response = logCurlRequest('PUT', url, body);

    assert(response.code === 201);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Matching an attribute which is a sub-document

@EXAMPLE_ARANGOSH_RUN{RestSimpleByExample2}
    var cn = "products";
    db._drop(cn);
    var products = db._create(cn);
    products.save({ "a": { "k": 1, "j": 1 }, "i": 1});
    products.save({ "a": { "j": 1 }, "i": 1});
    products.save({ "i": 1});
    products.save({ "a": { "k": 2, "j": 2 }, "i": 1});
    var url = "/_api/simple/by-example";
    var body = { "collection": "products", "example" : { "a.j" : 1 } };

    var response = logCurlRequest('PUT', url, body);

    assert(response.code === 201);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Matching an attribute within a sub-document

@EXAMPLE_ARANGOSH_RUN{RestSimpleByExample3}
    var cn = "products";
    db._drop(cn);
    var products = db._create(cn);
    products.save({ "a": { "k": 1, "j": 1 }, "i": 1});
    products.save({ "a": { "j": 1 }, "i": 1});
    products.save({ "i": 1});
    products.save({ "a": { "k": 2, "j": 2 }, "i": 1});
    var url = "/_api/simple/by-example";
    var body = { "collection": "products", "example" : { "a" : { "j" : 1 } } };

    var response = logCurlRequest('PUT', url, body);

    assert(response.code === 201);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
