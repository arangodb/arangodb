
@startDocuBlock REST_DOCUMENT_READ_ALL
@brief reads all documents from collection

@RESTHEADER{PUT /_api/simple/all-keys, Read all documents}

@RESTQUERYPARAMETERS

@RESTBODYPARAM{collection,string,optional,}
The name of the collection. This is only for backward compatibility.
In ArangoDB versions < 3.0, the URL path was */_api/document* and
this was passed in via the query parameter "collection".
This combination was removed.

@RESTBODYPARAM{type,string,optional,}
The type of the result. The following values are allowed:

  - *id*: returns an array of document ids (*_id* attributes)
  - *key*: returns an array of document keys (*_key* attributes)
  - *path*: returns an array of document URI paths. This is the default.

@RESTDESCRIPTION
Returns an array of all keys, ids, or URI paths for all documents in the
collection identified by *collection*. The type of the result array is
determined by the *type* attribute.

Note that the results have no defined order and thus the order should
not be relied on.

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

