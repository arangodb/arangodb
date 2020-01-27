
@startDocuBlock put_api_simple_all_keys
@brief reads all document keys from collection

@RESTHEADER{PUT /_api/simple/all-keys, Read all document keys, allDocumentKeys}

@HINTS
{% hint 'warning' %}
This route should no longer be used.
All endpoints for Simple Queries are deprecated from version 3.4.0 on.
They are superseded by AQL queries.
{% endhint %}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{collection,string,optional}
The name of the collection.
**This parameter is only for an easier migration path from old versions.**
In ArangoDB versions < 3.0, the URL path was `/_api/document` and
this was passed in via the query parameter "collection".
This combination was removed. The collection name can be passed to
`/_api/simple/all-keys` as body parameter (preferred) or as query parameter.

@RESTBODYPARAM{collection,string,required,}
The collection that should be queried

@RESTBODYPARAM{type,string,optional,}
The type of the result. The following values are allowed:<br>
  - *id*: returns an array of document ids (*_id* attributes)
  - *key*: returns an array of document keys (*_key* attributes)
  - *path*: returns an array of document URI paths. This is the default.

@RESTDESCRIPTION
Returns an array of all keys, ids, or URI paths for all documents in the
collection identified by *collection*. The type of the result array is
determined by the *type* attribute.

Note that the results have no defined order and thus the order should
not be relied on.

Note: the *all-keys* simple query is **deprecated** as of ArangoDB 3.4.0.
This API may get removed in future versions of ArangoDB. You can use the
`/_api/cursor` endpoint instead with one of the below AQL queries depending
on the desired result:

- `FOR doc IN @@collection RETURN doc._id` to mimic *type: id*
- `FOR doc IN @@collection RETURN doc._key` to mimic *type: key*
- `FOR doc IN @@collection RETURN CONCAT("/_db/", CURRENT_DATABASE(), "/_api/document/", doc._id)`
  to mimic *type: path*

@RESTRETURNCODES

@RESTRETURNCODE{201}
All went well.

@RESTRETURNCODE{404}
The collection does not exist.

@EXAMPLES

Return all document paths

@EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerReadDocumentAllPath}
    var cn = "products";
    db._drop(cn);
    db._create(cn);

    db.products.save({"hello1":"world1"});
    db.products.save({"hello2":"world1"});
    db.products.save({"hello3":"world1"});
    var url = "/_api/simple/all-keys";
    var body = {collection: cn};

    var response = logCurlRequest('PUT', url, body);

    assert(response.code === 201);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Return all document keys

@EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerReadDocumentAllKey}
    var cn = "products";
    db._drop(cn);
    db._create(cn);

    db.products.save({"hello1":"world1"});
    db.products.save({"hello2":"world1"});
    db.products.save({"hello3":"world1"});
    var url = "/_api/simple/all-keys";
    var body = {collection: cn, type: "id"};

    var response = logCurlRequest('PUT', url, body);

    assert(response.code === 201);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Collection does not exist

@EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerReadDocumentAllCollectionDoesNotExist}
    var cn = "doesnotexist";
    db._drop(cn);
    var url = "/_api/document/" + cn;

    var response = logCurlRequest('GET', url);

    assert(response.code === 404);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
