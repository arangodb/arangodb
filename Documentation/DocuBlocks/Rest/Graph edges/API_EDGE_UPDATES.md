
@startDocuBlock API_EDGE_UPDATES
@brief updates an edge

@RESTHEADER{PATCH /_api/document/{document-handle}#patchEdge, Patches edge}

@RESTALLBODYPARAM{document,json,required}
A JSON representation of the edge update.

@RESTURLPARAMETERS

@RESTURLPARAM{document-handle,string,required}
The handle of the edge document.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{keepNull,boolean,optional}
If the intention is to delete existing attributes with the patch command,
the URL query parameter *keepNull* can be used with a value of *false*.
This will modify the behavior of the patch command to remove any attributes
from the existing edge document that are contained in the patch document with an
attribute value of *null*.

@RESTQUERYPARAM{mergeObjects,boolean,optional}
Controls whether objects (not arrays) will be merged if present in both the
existing and the patch edge. If set to *false*, the value in the
patch edge will overwrite the existing edge's value. If set to
*true*, objects will be merged. The default is *true*.

@RESTQUERYPARAM{waitForSync,boolean,optional}
Wait until edge document has been synced to disk.

@RESTQUERYPARAM{rev,string,optional}
You can conditionally patch an edge document based on a target revision id by
using the *rev* query parameter.

@RESTQUERYPARAM{policy,string,optional}
To control the update behavior in case there is a revision mismatch, you
can use the *policy* parameter.

@RESTHEADERPARAMETERS

@RESTHEADERPARAM{If-Match,string,optional}
You can conditionally patch an edge document based on a target revision id by
using the *if-match* HTTP header.

@RESTDESCRIPTION
Partially updates the edge document identified by *document-handle*.
The body of the request must contain a JSON document with the attributes
to patch (the patch document). All attributes from the patch document will
be added to the existing edge document if they do not yet exist, and overwritten
in the existing edge document if they do exist there.

Setting an attribute value to *null* in the patch document will cause a
value of *null* be saved for the attribute by default.

**Note**: Internal attributes such as *_key*, *_from* and *_to* are immutable
once set and cannot be updated.

Optionally, the query parameter *waitForSync* can be used to force
synchronization of the edge document update operation to disk even in case
that the *waitForSync* flag had been disabled for the entire collection.
Thus, the *waitForSync* query parameter can be used to force synchronization
of just specific operations. To use this, set the *waitForSync* parameter
to *true*. If the *waitForSync* parameter is not specified or set to
*false*, then the collection's default *waitForSync* behavior is
applied. The *waitForSync* query parameter cannot be used to disable
synchronization for collections that have a default *waitForSync* value
of *true*.

The body of the response contains a JSON object with the information about
the handle and the revision. The attribute *_id* contains the known
*document-handle* of the updated edge document, *_key* contains the key which 
uniquely identifies a document in a given collection, and the attribute *_rev*
contains the new document revision.

If the edge document does not exist, then a *HTTP 404* is returned and the
body of the response contains an error document.

You can conditionally update an edge document based on a target revision id by
using either the *rev* query parameter or the *if-match* HTTP header.
To control the update behavior in case there is a revision mismatch, you
can use the *policy* parameter. This is the same as when replacing
edge documents (see replacing documents for details).

@RESTRETURNCODES

@RESTRETURNCODE{201}
is returned if the document was patched successfully and *waitForSync* was
*true*.

@RESTRETURNCODE{202}
is returned if the document was patched successfully and *waitForSync* was
*false*.

@RESTRETURNCODE{400}
is returned if the body does not contain a valid JSON representation or when
applied on an non-edge collection. The response body contains an error document
in this case.

@RESTRETURNCODE{404}
is returned if the collection or the edge document was not found

@RESTRETURNCODE{412}
is returned if a "If-Match" header or *rev* is given and the found
document has a different version. The response will also contain the found
document's current revision in the *_rev* attribute. Additionally, the
attributes *_id* and *_key* will be returned.
@endDocuBlock

