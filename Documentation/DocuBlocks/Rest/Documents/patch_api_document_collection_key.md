
@startDocuBlock patch_api_document_collection_key

@RESTHEADER{PATCH /_api/document/{collection}/{key},Update a document,updateDocument}

@RESTALLBODYPARAM{document,object,required}
A JSON representation of a document update as an object.

@RESTURLPARAMETERS

@RESTURLPARAM{collection,string,required}
Name of the `collection` in which the document is to be updated.

@RESTURLPARAM{key,string,required}
The document key.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{keepNull,boolean,optional}
If the intention is to delete existing attributes with the patch
command, set the `keepNull` URL query parameter to `false`. This modifies the
behavior of the patch command to remove top-level attributes and sub-attributes
from the existing document that are contained in the patch document with an
attribute value of `null` (but not attributes of objects that are nested inside
of arrays).

@RESTQUERYPARAM{mergeObjects,boolean,optional}
Controls whether objects (not arrays) are merged if present in
both the existing and the patch document. If set to `false`, the
value in the patch document overwrites the existing document's
value. If set to `true`, objects are merged. The default is
`true`.

@RESTQUERYPARAM{waitForSync,boolean,optional}
Wait until document has been synced to disk.

@RESTQUERYPARAM{ignoreRevs,boolean,optional}
By default, or if this is set to `true`, the `_rev` attributes in
the given document is ignored. If this is set to `false`, then
the `_rev` attribute given in the body document is taken as a
precondition. The document is only updated if the current revision
is the one specified.

@RESTQUERYPARAM{returnOld,boolean,optional}
Return additionally the complete previous revision of the changed
document under the attribute `old` in the result.

@RESTQUERYPARAM{returnNew,boolean,optional}
Return additionally the complete new document under the attribute `new`
in the result.

@RESTQUERYPARAM{silent,boolean,optional}
If set to `true`, an empty object is returned as response if the document operation
succeeds. No meta-data is returned for the updated document. If the
operation raises an error, an error object is returned.

You can use this option to save network traffic.

@RESTQUERYPARAM{refillIndexCaches,boolean,optional}
Whether to update existing entries in in-memory index caches if document updates
affect the edge index or cache-enabled persistent indexes.

@RESTHEADERPARAMETERS

@RESTHEADERPARAM{If-Match,string,optional}
You can conditionally update a document based on a target revision id by
using the `if-match` HTTP header.

@RESTDESCRIPTION
Partially updates the document identified by the *document ID*.
The body of the request must contain a JSON document with the
attributes to patch (the patch document). All attributes from the
patch document are added to the existing document if they do not
yet exist, and overwritten in the existing document if they do exist
there.

The value of the `_key` attribute as well as attributes
used as sharding keys may not be changed.

Setting an attribute value to `null` in the patch document causes a
value of `null` to be saved for the attribute by default.

If the `If-Match` header is specified and the revision of the
document in the database is unequal to the given revision, the
precondition is violated.

If `If-Match` is not given and `ignoreRevs` is `false` and there
is a `_rev` attribute in the body and its value does not match
the revision of the document in the database, the precondition is
violated.

If a precondition is violated, an *HTTP 412* is returned.

If the document exists and can be updated, then an *HTTP 201* or
an *HTTP 202* is returned (depending on `waitForSync`, see below),
the `ETag` header field contains the new revision of the document
(in double quotes) and the `Location` header contains a complete URL
under which the document can be queried.

Cluster only: The patch document _may_ contain
values for the collection's pre-defined shard keys. Values for the shard keys
are treated as hints to improve performance. Should the shard keys
values be incorrect ArangoDB may answer with a `not found` error

Optionally, the query parameter `waitForSync` can be used to force
synchronization of the updated document operation to disk even in case
that the `waitForSync` flag had been disabled for the entire collection.
Thus, the `waitForSync` query parameter can be used to force synchronization
of just specific operations. To use this, set the `waitForSync` parameter
to `true`. If the `waitForSync` parameter is not specified or set to
`false`, then the collection's default `waitForSync` behavior is
applied. The `waitForSync` query parameter cannot be used to disable
synchronization for collections that have a default `waitForSync` value
of `true`.

If `silent` is not set to `true`, the body of the response contains a JSON
object with the information about the identifier and the revision. The attribute
`_id` contains the known *document ID* of the updated document, `_key`
contains the key which uniquely identifies a document in a given collection,
and the attribute `_rev` contains the new document revision.

If the query parameter `returnOld` is `true`, then
the complete previous revision of the document
is returned under the `old` attribute in the result.

If the query parameter `returnNew` is `true`, then
the complete new document is returned under
the `new` attribute in the result.

If the document does not exist, then a *HTTP 404* is returned and the
body of the response contains an error document.

@RESTRETURNCODES

@RESTRETURNCODE{201}
is returned if the document was updated successfully and
`waitForSync` was `true`.

@RESTRETURNCODE{202}
is returned if the document was updated successfully and
`waitForSync` was `false`.

@RESTRETURNCODE{400}
is returned if the body does not contain a valid JSON representation
of a document. The response body contains
an error document in this case.

@RESTRETURNCODE{403}
with the error code `1004` is returned if the specified write concern for the
collection cannot be fulfilled. This can happen if less than the number of
specified replicas for a shard are currently in-sync with the leader. For example,
if the write concern is `2` and the replication factor is `3`, then the
write concern is not fulfilled if two replicas are not in-sync.

Note that the HTTP status code is configurable via the
`--cluster.failed-write-concern-status-code` startup option. It defaults to `403`
but can be changed to `503` to signal client applications that it is a
temporary error.

@RESTRETURNCODE{404}
is returned if the collection or the document was not found.

@RESTRETURNCODE{409}
There are two possible reasons for this error:

- The update causes a unique constraint violation in a secondary index.
  The response body contains an error document with the `errorNum` set to
  `1210` (`ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED`) in this case.
- Locking the document key or some unique index entry failed due to another
  concurrent operation that operates on the same document. This is also referred
  to as a _write-write conflict_. The response body contains an error document
  with the `errorNum` set to `1200` (`ERROR_ARANGO_CONFLICT`) in this case.

@RESTRETURNCODE{412}
is returned if the precondition was violated. The response also contains
the found documents' current revisions in the `_rev` attributes.
Additionally, the attributes `_id` and `_key` are returned.

@RESTRETURNCODE{503}
is returned if the system is temporarily not available. This can be a system
overload or temporary failure. In this case it makes sense to retry the request
later.

If the error code is `1429`, then the write concern for the collection cannot be
fulfilled. This can happen if less than the number of specified replicas for
a shard are currently in-sync with the leader. For example, if the write concern
is `2` and the replication factor is `3`, then the write concern is not fulfilled
if two replicas are not in-sync.

Note that the HTTP status code is configurable via the
`--cluster.failed-write-concern-status-code` startup option. It defaults to `403`
but can be changed to `503` to signal client applications that it is a
temporary error.

@EXAMPLES

Patches an existing document with new content.

@EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerPatchDocument}
    var cn = "products";
    db._drop(cn);
    db._create(cn);

    var document = db.products.save({"one":"world"});
    var url = "/_api/document/" + document._id;

    var response = logCurlRequest("PATCH", url, { "hello": "world" });

    assert(response.code === 202);

    logJsonResponse(response);
    var response2 = logCurlRequest("PATCH", url, { "numbers": { "one": 1, "two": 2, "three": 3, "empty": null } });
    assert(response2.code === 202);
    logJsonResponse(response2);
    var response3 = logCurlRequest("GET", url);
    assert(response3.code === 200);
    logJsonResponse(response3);
    var response4 = logCurlRequest("PATCH", url + "?keepNull=false", { "hello": null, "numbers": { "four": 4 } });
    assert(response4.code === 202);
    logJsonResponse(response4);
    var response5 = logCurlRequest("GET", url);
    assert(response5.code === 200);
    logJsonResponse(response5);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Merging attributes of an object using `mergeObjects`:

@EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerPatchDocumentMerge}
    var cn = "products";
    db._drop(cn);
    db._create(cn);

    var document = db.products.save({"inhabitants":{"china":1366980000,"india":1263590000,"usa":319220000}});
    var url = "/_api/document/" + document._id;

    var response = logCurlRequest("GET", url);
    assert(response.code === 200);
    logJsonResponse(response);

    var response = logCurlRequest("PATCH", url + "?mergeObjects=true", { "inhabitants": {"indonesia":252164800,"brazil":203553000 }});
    assert(response.code === 202);

    var response2 = logCurlRequest("GET", url);
    assert(response2.code === 200);
    logJsonResponse(response2);

    var response3 = logCurlRequest("PATCH", url + "?mergeObjects=false", { "inhabitants": { "pakistan":188346000 }});
    assert(response3.code === 202);
    logJsonResponse(response3);

    var response4 = logCurlRequest("GET", url);
    assert(response4.code === 200);
    logJsonResponse(response4);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
