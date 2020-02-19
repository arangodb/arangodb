@startDocuBlock put_api_simple_update_by_example
@brief partially updates the body of all documents of a collection that match an example

@RESTHEADER{PUT /_api/simple/update-by-example, Update documents by example}

@HINTS
{% hint 'warning' %}
This route should no longer be used.
All endpoints for Simple Queries are deprecated from version 3.4.0 on.
They are superseded by AQL queries.
{% endhint %}

@RESTBODYPARAM{collection,string,required,string}
The name of the collection to update within.

@RESTBODYPARAM{example,string,required,string}
An example document that all collection documents are compared against.

@RESTBODYPARAM{newValue,object,required,}
A document containing all the attributes to update in the found documents.

@RESTBODYPARAM{options,object,optional,put_api_simple_update_by_example_options}
a json object which can contains following attributes:

@RESTSTRUCT{keepNull,put_api_simple_update_by_example_options,boolean,optional,}
This parameter can be used to modify the behavior when
handling *null* values. Normally, *null* values are stored in the
database. By setting the *keepNull* parameter to *false*, this
behavior can be changed so that all attributes in *data* with *null*
values will be removed from the updated document.

@RESTSTRUCT{waitForSync,put_api_simple_update_by_example_options,boolean,optional,}
if set to true, then all removal operations will
instantly be synchronized to disk. If this is not specified, then the
collection's default sync behavior will be applied.

@RESTSTRUCT{limit,put_api_simple_update_by_example_options,integer,optional,int64}
an optional value that determines how many documents to
update at most. If *limit* is specified but is less than the number
of documents in the collection, it is undefined which of the documents
will be updated.

@RESTSTRUCT{mergeObjects,put_api_simple_update_by_example_options,boolean,optional,}
Controls whether objects (not arrays) will be merged if present in both the
existing and the patch document. If set to false, the value in the
patch document will overwrite the existing document's value. If set to
true, objects will be merged. The default is true.

@RESTDESCRIPTION

This will find all documents in the collection that match the specified
example object, and partially update the document body with the new value
specified. Note that document meta-attributes such as *_id*, *_key*,
*_from*, *_to* etc. cannot be replaced.

Note: the *limit* attribute is not supported on sharded collections.
Using it will result in an error.

Returns the number of documents that were updated.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the collection was updated successfully and *waitForSync* was
*true*.

@RESTRETURNCODE{400}
is returned if the body does not contain a valid JSON representation of a
query. The response body contains an error document in this case.

@RESTRETURNCODE{404}
is returned if the collection specified by *collection* is unknown.  The
response body contains an error document in this case.

@EXAMPLES

using old syntax for options

@EXAMPLE_ARANGOSH_RUN{RestSimpleUpdateByExample}
    var cn = "products";
    db._drop(cn);
    var products = db._create(cn);
    products.save({ "a": { "k": 1, "j": 1 }, "i": 1});
    products.save({ "a": { "j": 1 }, "i": 1});
    products.save({ "i": 1});
    products.save({ "a": { "k": 2, "j": 2 }, "i": 1});
    var url = "/_api/simple/update-by-example";
    var body = {
      "collection": "products",
      "example" : { "a" : { "j" : 1 } },
      "newValue" : { "a" : { "j" : 22 } },
      "limit" : 3
    };

    var response = logCurlRequest('PUT', url, body);

    assert(response.code === 200);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

using new signature for options

@EXAMPLE_ARANGOSH_RUN{RestSimpleUpdateByExample_1}
    var cn = "products";
    db._drop(cn);
    var products = db._create(cn);
    products.save({ "a": { "k": 1, "j": 1 }, "i": 1});
    products.save({ "a": { "j": 1 }, "i": 1});
    products.save({ "i": 1});
    products.save({ "a": { "k": 2, "j": 2 }, "i": 1});
    var url = "/_api/simple/update-by-example";
    var body = {
      "collection": "products",
      "example" : { "a" : { "j" : 1 } },
      "newValue" : { "a" : { "j" : 22 } },
      "options" :  { "limit" : 3, "waitForSync": true }
    };

    var response = logCurlRequest('PUT', url, body);

    assert(response.code === 200);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
