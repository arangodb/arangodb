
@startDocuBlock API_EDGE_DELETE
@brief deletes an edge

@RESTHEADER{DELETE /_api/document/{document-handle}, Deletes edge}

@RESTURLPARAMETERS

@RESTURLPARAM{document-handle,string,required}
Deletes the edge document identified by *document-handle*.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{rev,string,optional}
You can conditionally delete an edge document based on a target revision id by
using the *rev* query parameter.

@RESTQUERYPARAM{policy,string,optional}
To control the update behavior in case there is a revision mismatch, you
can use the *policy* parameter. This is the same as when replacing edge
documents (see replacing edge documents for more details).

@RESTQUERYPARAM{waitForSync,boolean,optional}
Wait until edge document has been synced to disk.

@RESTHEADERPARAMETERS

@RESTHEADERPARAM{If-Match,string,optional}
You can conditionally delete an edge document based on a target revision id by
using the *if-match* HTTP header.

@RESTDESCRIPTION
The body of the response contains a JSON object with the information about
the handle and the revision. The attribute *_id* contains the known
*document-handle* of the deleted edge document, *_key* contains the key which 
uniquely identifies a document in a given collection, and the attribute *_rev*
contains the new document revision.

If the *waitForSync* parameter is not specified or set to
*false*, then the collection's default *waitForSync* behavior is
applied. The *waitForSync* query parameter cannot be used to disable
synchronization for collections that have a default *waitForSync* value
of *true*.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the edge document was deleted successfully and *waitForSync* was
*true*.

@RESTRETURNCODE{202}
is returned if the edge document was deleted successfully and *waitForSync* was
*false*.

@RESTRETURNCODE{404}
is returned if the collection or the edge document was not found.
The response body contains an error document in this case.

@RESTRETURNCODE{412}
is returned if a "If-Match" header or *rev* is given and the found
document has a different version. The response will also contain the found
document's current revision in the *_rev* attribute. Additionally, the
attributes *_id* and *_key* will be returned.
@endDocuBlock

