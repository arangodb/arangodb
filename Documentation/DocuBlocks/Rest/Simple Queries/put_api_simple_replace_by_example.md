@startDocuBlock put_api_simple_replace_by_example
@brief replaces the body of all documents of a collection that match an example

@RESTHEADER{PUT /_api/simple/replace-by-example, Replace documents by example}

@HINTS
{% hint 'warning' %}
This route should no longer be used.
All endpoints for Simple Queries are deprecated from version 3.4.0 on.
They are superseded by AQL queries.
{% endhint %}

@RESTBODYPARAM{collection,string,required,string}
The name of the collection to replace within.

@RESTBODYPARAM{example,string,required,string}
An example document that all collection documents are compared against.

@RESTBODYPARAM{newValue,string,required,string}
The replacement document that will get inserted in place
of the "old" documents.

@RESTBODYPARAM{options,object,optional,put_api_simple_replace_by_example_options}
a json object which can contain following attributes

@RESTSTRUCT{waitForSync,put_api_simple_replace_by_example_options,boolean,optional,}
if set to true, then all removal operations will
 instantly be synchronized to disk. If this is not specified, then the
 collection's default sync behavior will be applied.

@RESTSTRUCT{limit,put_api_simple_replace_by_example_options,string,optional,string}
an optional value that determines how many documents to
replace at most. If *limit* is specified but is less than the number
of documents in the collection, it is undefined which of the documents
will be replaced.

@RESTDESCRIPTION

This will find all documents in the collection that match the specified
example object, and replace the entire document body with the new value
specified. Note that document meta-attributes such as *_id*, *_key*,
*_from*, *_to* etc. cannot be replaced.

Note: the *limit* attribute is not supported on sharded collections.
Using it will result in an error.

Returns the number of documents that were replaced.

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

@EXAMPLE_ARANGOSH_RUN{RestSimpleReplaceByExample}
    var cn = "products";
    db._drop(cn);
    var products = db._create(cn);
    products.save({ "a": { "k": 1, "j": 1 }, "i": 1});
    products.save({ "a": { "j": 1 }, "i": 1});
    products.save({ "i": 1});
    products.save({ "a": { "k": 2, "j": 2 }, "i": 1});
    var url = "/_api/simple/replace-by-example";
    var body = {
      "collection": "products",
      "example" : { "a" : { "j" : 1 } },
      "newValue" : {"foo" : "bar"},
      "limit" : 3
    };

    var response = logCurlRequest('PUT', url, body);

    assert(response.code === 200);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Using new Signature for attributes WaitForSync and limit

@EXAMPLE_ARANGOSH_RUN{RestSimpleReplaceByExampleWaitForSync}
    var cn = "products";
    db._drop(cn);
    var products = db._create(cn);
    products.save({ "a": { "k": 1, "j": 1 }, "i": 1});
    products.save({ "a": { "j": 1 }, "i": 1});
    products.save({ "i": 1});
    products.save({ "a": { "k": 2, "j": 2 }, "i": 1});
    var url = "/_api/simple/replace-by-example";
    var body = {
      "collection": "products",
      "example" : { "a" : { "j" : 1 } },
      "newValue" : {"foo" : "bar"},
      "options": {"limit" : 3,  "waitForSync": true  }
    };

    var response = logCurlRequest('PUT', url, body);

    assert(response.code === 200);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
