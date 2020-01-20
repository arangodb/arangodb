
@startDocuBlock put_api_simple_remove_by_example
@brief removes all documents of a collection that match an example

@RESTHEADER{PUT /_api/simple/remove-by-example, Remove documents by example}

@HINTS
{% hint 'warning' %}
This route should no longer be used.
All endpoints for Simple Queries are deprecated from version 3.4.0 on.
They are superseded by AQL queries.
{% endhint %}

@RESTBODYPARAM{collection,string,required,string}
The name of the collection to remove from.

@RESTBODYPARAM{example,string,required,string}
An example document that all collection documents are compared against.

@RESTBODYPARAM{options,object,optional,put_api_simple_remove_by_example_opts}
a json object which can contains following attributes:

@RESTSTRUCT{waitForSync,put_api_simple_remove_by_example_opts,boolean,optional,}
if set to true, then all removal operations will
instantly be synchronized to disk. If this is not specified, then the
collection's default sync behavior will be applied.

@RESTSTRUCT{limit,put_api_simple_remove_by_example_opts,string,required,string}
an optional value that determines how many documents to
delete at most. If *limit* is specified but is less than the number
of documents in the collection, it is undefined which of the documents
will be deleted.

@RESTDESCRIPTION

This will find all documents in the collection that match the specified
example object.

Note: the *limit* attribute is not supported on sharded collections.
Using it will result in an error.

Returns the number of documents that were deleted.

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

@EXAMPLE_ARANGOSH_RUN{RestSimpleRemoveByExample}
    var cn = "products";
    db._drop(cn);
    var products = db._create(cn);
    products.save({ "a": { "k": 1, "j": 1 }, "i": 1});
    products.save({ "a": { "j": 1 }, "i": 1});
    products.save({ "i": 1});
    products.save({ "a": { "k": 2, "j": 2 }, "i": 1});
    var url = "/_api/simple/remove-by-example";
    var body = { "collection": "products", "example" : { "a" : { "j" : 1 } } };

    var response = logCurlRequest('PUT', url, body);

    assert(response.code === 200);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Using Parameter: waitForSync and limit

@EXAMPLE_ARANGOSH_RUN{RestSimpleRemoveByExample_1}
    var cn = "products";
    db._drop(cn);
    var products = db._create(cn);
    products.save({ "a": { "k": 1, "j": 1 }, "i": 1});
    products.save({ "a": { "j": 1 }, "i": 1});
    products.save({ "i": 1});
    products.save({ "a": { "k": 2, "j": 2 }, "i": 1});
    var url = "/_api/simple/remove-by-example";
    var body = { "collection": "products", "example" : { "a" : { "j" : 1 } },
                "waitForSync": true, "limit": 2 };

    var response = logCurlRequest('PUT', url, body);

    assert(response.code === 200);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Using Parameter: waitForSync and limit with new signature

@EXAMPLE_ARANGOSH_RUN{RestSimpleRemoveByExample_2}
    var cn = "products";
    db._drop(cn);
    var products = db._create(cn);
    products.save({ "a": { "k": 1, "j": 1 }, "i": 1});
    products.save({ "a": { "j": 1 }, "i": 1});
    products.save({ "i": 1});
    products.save({ "a": { "k": 2, "j": 2 }, "i": 1});
    var url = "/_api/simple/remove-by-example";
    var body = {
               "collection": "products",
               "example" : { "a" : { "j" : 1 } },
               "options": {"waitForSync": true, "limit": 2}
               };

    var response = logCurlRequest('PUT', url, body);

    assert(response.code === 200);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
