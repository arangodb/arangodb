@startDocuBlock RestRemoveByKeys
@brief removes multiple documents by their keys

@RESTHEADER{PUT /_api/simple/remove-by-keys, Remove documents by their keys}

@HINTS
{% hint 'warning' %}
This route should no longer be used.
All endpoints for Simple Queries are deprecated from version 3.4.0 on.
They are superseded by AQL queries.
{% endhint %}

@RESTBODYPARAM{collection,string,required,string}
The name of the collection to look in for the documents to remove

@RESTBODYPARAM{keys,array,required,string}
array with the _keys of documents to remove.

@RESTBODYPARAM{options,object,optional,put_api_simple_remove_by_keys_opts}
a json object which can contains following attributes:

@RESTSTRUCT{waitForSync,put_api_simple_remove_by_keys_opts,boolean,optional,}
if set to true, then all removal operations will
instantly be synchronized to disk. If this is not specified, then the
collection's default sync behavior will be applied.

@RESTSTRUCT{silent,put_api_simple_remove_by_keys_opts,boolean,optional,}
if set to *false*, then the result will contain an additional
attribute *old* which contains an array with one entry for each
removed document. By default, these entries will have the *_id*,
*_key* and *_rev* attributes.

@RESTSTRUCT{returnOld,put_api_simple_remove_by_keys_opts,boolean,optional,}
if set to *true* and *silent* above is *false*, then the above
information about the removed documents contains the complete
removed documents.

@RESTDESCRIPTION
Looks up the documents in the specified collection using the array of keys
provided, and removes all documents from the collection whose keys are
contained in the *keys* array. Keys for which no document can be found in
the underlying collection are ignored, and no exception will be thrown for
them.

Equivalent AQL query (the RETURN clause is optional):

    FOR key IN @keys REMOVE key IN @@collection
      RETURN OLD

The body of the response contains a JSON object with information how many
documents were removed (and how many were not). The *removed* attribute will
contain the number of actually removed documents. The *ignored* attribute
will contain the number of keys in the request for which no matching document
could be found.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the operation was carried out successfully. The number of removed
documents may still be 0 in this case if none of the specified document keys
were found in the collection.

@RESTRETURNCODE{404}
is returned if the collection was not found.
The response body contains an error document in this case.

@RESTRETURNCODE{405}
is returned if the operation was called with a different HTTP METHOD than PUT.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestSimpleRemove}
    var cn = "test";
  ~ db._drop(cn);
    db._create(cn);
    keys = [ ];
    for (var i = 0; i < 10; ++i) {
      db.test.insert({ _key: "test" + i });
      keys.push("test" + i);
    }

    var url = "/_api/simple/remove-by-keys";
    var data = { keys: keys, collection: cn };
    var response = logCurlRequest('PUT', url, data);

    assert(response.code === 200);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

@EXAMPLE_ARANGOSH_RUN{RestSimpleRemoveNotFound}
    var cn = "test";
  ~ db._drop(cn);
    db._create(cn);
    keys = [ ];
    for (var i = 0; i < 10; ++i) {
      db.test.insert({ _key: "test" + i });
    }

    var url = "/_api/simple/remove-by-keys";
    var data = { keys: [ "foo", "bar", "baz" ], collection: cn };
    var response = logCurlRequest('PUT', url, data);

    assert(response.code === 200);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
