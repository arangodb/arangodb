@startDocuBlock delete_api_document_collection_key
@brief removes a document

@RESTHEADER{DELETE /_api/document/{collection}/{key},Removes a document,deleteDocument}

@RESTURLPARAMETERS

@RESTURLPARAM{collection,string,required}
Name of the `collection` in which the document is to be deleted.

@RESTURLPARAM{key,string,required}
The document key.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{waitForSync,boolean,optional}
Wait until deletion operation has been synced to disk.

@RESTQUERYPARAM{returnOld,boolean,optional}
Return additionally the complete previous revision of the changed
document under the attribute `old` in the result.

@RESTQUERYPARAM{silent,boolean,optional}
If set to `true`, an empty object is returned as response if the document operation
succeeds. No meta-data is returned for the deleted document. If the
operation raises an error, an error object is returned.

You can use this option to save network traffic.

@RESTQUERYPARAM{refillIndexCaches,boolean,optional}
Whether to delete existing entries from in-memory index caches and refill them
if document removals affect the edge index or cache-enabled persistent indexes.

@RESTHEADERPARAMETERS

@RESTHEADERPARAM{If-Match,string,optional}
You can conditionally remove a document based on a target revision id by
using the `if-match` HTTP header.

@RESTDESCRIPTION
If `silent` is not set to `true`, the body of the response contains a JSON
object with the information about the identifier and the revision. The attribute
`_id` contains the known *document ID* of the removed document, `_key`
contains the key which uniquely identifies a document in a given collection,
and the attribute `_rev` contains the document revision.

If the `waitForSync` parameter is not specified or set to `false`,
then the collection's default `waitForSync` behavior is applied.
The `waitForSync` query parameter cannot be used to disable
synchronization for collections that have a default `waitForSync`
value of `true`.

If the query parameter `returnOld` is `true`, then
the complete previous revision of the document
is returned under the `old` attribute in the result.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the document was removed successfully and
`waitForSync` was `true`.

@RESTRETURNCODE{202}
is returned if the document was removed successfully and
`waitForSync` was `false`.

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
The response body contains an error document in this case.

@RESTRETURNCODE{409}
is returned if locking the document key failed due to another
concurrent operation that operates on the same document.
This is also referred to as a _write-write conflict_.
The response body contains an error document with the
`errorNum` set to `1200` (`ERROR_ARANGO_CONFLICT`) in this case.

@RESTRETURNCODE{412}
is returned if a "If-Match" header or `rev` is given and the found
document has a different version. The response also contain the found
document's current revision in the `_rev` attribute. Additionally, the
attributes `_id` and `_key` are returned.

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

Using document identifier:

@EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerDeleteDocument}
    var cn = "products";
    db._drop(cn);
    db._create(cn, { waitForSync: true });
    var document = db.products.save({"hello":"world"});

    var url = "/_api/document/" + document._id;

    var response = logCurlRequest('DELETE', url);

    assert(response.code === 200);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Unknown document identifier:

@EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerDeleteDocumentUnknownHandle}
    var cn = "products";
    db._drop(cn);
    db._create(cn, { waitForSync: true });
    var document = db.products.save({"hello":"world"});
    db.products.remove(document._id);

    var url = "/_api/document/" + document._id;

    var response = logCurlRequest('DELETE', url);

    assert(response.code === 404);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Revision conflict:

@EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerDeleteDocumentIfMatchOther}
    var cn = "products";
    db._drop(cn);
    db._create(cn);

    var document = db.products.save({"hello":"world"});
    var document2 = db.products.save({"hello2":"world"});
    var url = "/_api/document/" + document._id;
    var headers = {"If-Match":  "\"" + document2._rev + "\""};

    var response = logCurlRequest('DELETE', url, "", headers);

    assert(response.code === 412);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
