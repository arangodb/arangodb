@startDocuBlock RestLookupByKeys
@brief fetches multiple documents by their keys

@RESTHEADER{PUT /_api/simple/lookup-by-keys, Find documents by their keys}

@HINTS
{% hint 'warning' %}
This route should no longer be used.
All endpoints for Simple Queries are deprecated from version 3.4.0 on.
They are superseded by AQL queries.
{% endhint %}

@RESTBODYPARAM{collection,string,required,string}
The name of the collection to look in for the documents

@RESTBODYPARAM{keys,array,required,string}
array with the _keys of documents to remove.

@RESTDESCRIPTION
Looks up the documents in the specified collection
using the array of keys provided. All documents for which a matching
key was specified in the *keys* array and that exist in the collection
will be returned.  Keys for which no document can be found in the
underlying collection are ignored, and no exception will be thrown for
them.

Equivalent AQL query:

    FOR doc IN @@collection FILTER doc._key IN @keys RETURN doc

The body of the response contains a JSON object with a *documents*
attribute. The *documents* attribute is an array containing the
matching documents. The order in which matching documents are present
in the result array is unspecified.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the operation was carried out successfully.

@RESTRETURNCODE{404}
is returned if the collection was not found.  The response body
contains an error document in this case.

@RESTRETURNCODE{405}
is returned if the operation was called with a different HTTP METHOD than PUT.

@EXAMPLES

Looking up existing documents

@EXAMPLE_ARANGOSH_RUN{RestSimpleLookup}
    var cn = "test";
  ~ db._drop(cn);
    db._create(cn);
    keys = [ ];
    for (i = 0; i < 10; ++i) {
      db.test.insert({ _key: "test" + i, value: i });
      keys.push("test" + i);
    }

    var url = "/_api/simple/lookup-by-keys";
    var data = { keys: keys, collection: cn };
    var response = logCurlRequest('PUT', url, data);

    assert(response.code === 200);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Looking up non-existing documents

@EXAMPLE_ARANGOSH_RUN{RestSimpleLookupNotFound}
    var cn = "test";
  ~ db._drop(cn);
    db._create(cn);
    keys = [ ];
    for (i = 0; i < 10; ++i) {
      db.test.insert({ _key: "test" + i, value: i });
    }

    var url = "/_api/simple/lookup-by-keys";
    var data = { keys: [ "foo", "bar", "baz" ], collection: cn };
    var response = logCurlRequest('PUT', url, data);

    assert(response.code === 200);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
