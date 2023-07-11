@startDocuBlock post_api_document_collection_multiple

@RESTHEADER{POST /_api/document/{collection}#multiple,Create multiple documents,createDocuments}

@RESTURLPARAMETERS

@RESTURLPARAM{collection,string,required}
Name of the `collection` in which the documents are to be created.

@RESTALLBODYPARAM{documents,json,required}
An array of documents to create.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{collection,string,optional}
The name of the collection. This is only for backward compatibility.
In ArangoDB versions < 3.0, the URL path was `/_api/document` and
this query parameter was required. This combination still works, but
the recommended way is to specify the collection in the URL path.

@RESTQUERYPARAM{waitForSync,boolean,optional}
Wait until document has been synced to disk.

@RESTQUERYPARAM{returnNew,boolean,optional}
Additionally return the complete new document under the attribute `new`
in the result.

@RESTQUERYPARAM{returnOld,boolean,optional}
Additionally return the complete old document under the attribute `old`
in the result. Only available if the overwrite option is used.

@RESTQUERYPARAM{silent,boolean,optional}
If set to `true`, an empty object is returned as response if all document operations
succeed. No meta-data is returned for the created documents. If any of the
operations raises an error, an array with the error object(s) is returned.

You can use this option to save network traffic but you cannot map any errors
to the inputs of your request.

@RESTQUERYPARAM{overwrite,boolean,optional}
If set to `true`, the insert becomes a replace-insert. If a document with the
same `_key` already exists, the new document is not rejected with a unique
constraint violation error but replaces the old document. Note that operations
with `overwrite` parameter require a `_key` attribute in the request payload,
therefore they can only be performed on collections sharded by `_key`.

@RESTQUERYPARAM{overwriteMode,string,optional}
This option supersedes `overwrite` and offers the following modes:
- `"ignore"`: if a document with the specified `_key` value exists already,
  nothing is done and no write operation is carried out. The
  insert operation returns success in this case. This mode does not
  support returning the old document version using `RETURN OLD`. When using
  `RETURN NEW`, `null` is returned in case the document already existed.
- `"replace"`: if a document with the specified `_key` value exists already,
  it is overwritten with the specified document value. This mode is
  also used when no overwrite mode is specified but the `overwrite`
  flag is set to `true`.
- `"update"`: if a document with the specified `_key` value exists already,
  it is patched (partially updated) with the specified document value.
  The overwrite mode can be further controlled via the `keepNull` and
  `mergeObjects` parameters.
- `"conflict"`: if a document with the specified `_key` value exists already,
  return a unique constraint violation error so that the insert operation
  fails. This is also the default behavior in case the overwrite mode is
  not set, and the `overwrite` flag is `false` or not set either.

@RESTQUERYPARAM{keepNull,boolean,optional}
If the intention is to delete existing attributes with the update-insert
command, set the `keepNull` URL query parameter to `false`. This modifies the
behavior of the patch command to remove top-level attributes and sub-attributes
from the existing document that are contained in the patch document with an
attribute value of `null` (but not attributes of objects that are nested inside
of arrays). This option controls the update-insert behavior only.

@RESTQUERYPARAM{mergeObjects,boolean,optional}
Controls whether objects (not arrays) are merged if present in both, the
existing and the update-insert document. If set to `false`, the value in the
patch document overwrites the existing document's value. If set to `true`,
objects are merged. The default is `true`.
This option controls the update-insert behavior only.

@RESTQUERYPARAM{refillIndexCaches,boolean,optional}
Whether to add new entries to the in-memory edge cache if edge documents are
inserted.

@RESTDESCRIPTION
Creates new documents from the documents given in the body, unless there
is already a document with the `_key` given. If no `_key` is given, a new
unique `_key` is generated automatically.

The result body contains a JSON array of the
same length as the input array, and each entry contains the result
of the operation for the corresponding input. In case of an error
the entry is a document with attributes `error` set to `true` and
errorCode set to the error code that has happened.

Possibly given `_id` and `_rev` attributes in the body are always ignored,
the URL part or the query parameter collection respectively counts.

If `silent` is not set to `true`, the body of the response contains an
array of JSON objects with the following attributes:

  - `_id` contains the document identifier of the newly created document
  - `_key` contains the document key
  - `_rev` contains the document revision

If the collection parameter `waitForSync` is `false`, then the call
returns as soon as the documents have been accepted. It does not wait
until the documents have been synced to disk.

Optionally, the query parameter `waitForSync` can be used to force
synchronization of the document creation operation to disk even in
case that the `waitForSync` flag had been disabled for the entire
collection. Thus, the `waitForSync` query parameter can be used to
force synchronization of just this specific operations. To use this,
set the `waitForSync` parameter to `true`. If the `waitForSync`
parameter is not specified or set to `false`, then the collection's
default `waitForSync` behavior is applied. The `waitForSync` query
parameter cannot be used to disable synchronization for collections
that have a default `waitForSync` value of `true`.

If the query parameter `returnNew` is `true`, then, for each
generated document, the complete new document is returned under
the `new` attribute in the result.

Should an error have occurred with some of the documents
the additional HTTP header `X-Arango-Error-Codes` is set, which
contains a map of the error codes that occurred together with their
multiplicities, as in: `1205:10,1210:17` which means that in 10
cases the error 1205 "illegal document handle" and in 17 cases the
error 1210 "unique constraint violated" has happened.

@RESTRETURNCODES

@RESTRETURNCODE{201}
is returned if `waitForSync` was `true` and operations were processed.

@RESTRETURNCODE{202}
is returned if `waitForSync` was `false` and operations were processed.

@RESTRETURNCODE{400}
is returned if the body does not contain a valid JSON representation
of an array of documents. The response body contains
an error document in this case.

@RESTRETURNCODE{404}
is returned if the collection specified by `collection` is unknown.
The response body contains an error document in this case.

@EXAMPLES

Insert multiple documents:

@EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerPostMulti1}
    var cn = "products";
    db._drop(cn);
    db._create(cn);

    var url = "/_api/document/" + cn;
    var body = '[{"Hello":"Earth"}, {"Hello":"Venus"}, {"Hello":"Mars"}]';

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 202);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Use of returnNew:

@EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerPostMulti2}
    var cn = "products";
    db._drop(cn);
    db._create(cn);

    var url = "/_api/document/" + cn + "?returnNew=true";
    var body = '[{"Hello":"Earth"}, {"Hello":"Venus"}, {"Hello":"Mars"}]';

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 202);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Partially illegal documents:

@EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerPostBadJsonMulti}
    var cn = "products";
    db._drop(cn);
    db._create(cn);

    var url = "/_api/document/" + cn;
    var body = '[{ "_key": 111 }, {"_key":"abc"}]';

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 202);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
