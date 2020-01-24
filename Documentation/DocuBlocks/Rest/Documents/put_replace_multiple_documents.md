
@startDocuBlock put_replace_document_MULTI
@brief replaces multiple documents

@RESTHEADER{PUT /_api/document/{collection},Replace documents,replaceDocuments}

@RESTALLBODYPARAM{documents,json,required}
A JSON representation of an array of documents.

@RESTURLPARAMETERS

@RESTURLPARAM{collection,string,required}
This URL parameter is the name of the collection in which the
documents are replaced.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{waitForSync,boolean,optional}
Wait until the new documents have been synced to disk.

@RESTQUERYPARAM{ignoreRevs,boolean,optional}
By default, or if this is set to *true*, the *_rev* attributes in
the given documents are ignored. If this is set to *false*, then
any *_rev* attribute given in a body document is taken as a
precondition. The document is only replaced if the current revision
is the one specified.

@RESTQUERYPARAM{returnOld,boolean,optional}
Return additionally the complete previous revision of the changed
documents under the attribute *old* in the result.

@RESTQUERYPARAM{returnNew,boolean,optional}
Return additionally the complete new documents under the attribute *new*
in the result.

@RESTDESCRIPTION
Replaces multiple documents in the specified collection with the
ones in the body, the replaced documents are specified by the *_key*
attributes in the body documents.

The value of the `_key` attribute as well as attributes
used as sharding keys may not be changed.

If *ignoreRevs* is *false* and there is a *_rev* attribute in a
document in the body and its value does not match the revision of
the corresponding document in the database, the precondition is
violated.

Cluster only: The replace documents _may_ contain
values for the collection's pre-defined shard keys. Values for the shard keys
are treated as hints to improve performance. Should the shard keys
values be incorrect ArangoDB may answer with a *not found* error.

Optionally, the query parameter *waitForSync* can be used to force
synchronization of the document replacement operation to disk even in case
that the *waitForSync* flag had been disabled for the entire collection.
Thus, the *waitForSync* query parameter can be used to force synchronization
of just specific operations. To use this, set the *waitForSync* parameter
to *true*. If the *waitForSync* parameter is not specified or set to
*false*, then the collection's default *waitForSync* behavior is
applied. The *waitForSync* query parameter cannot be used to disable
synchronization for collections that have a default *waitForSync* value
of *true*.

The body of the response contains a JSON array of the same length
as the input array with the information about the identifier and the
revision of the replaced documents. In each entry, the attribute
*_id* contains the known *document-id* of each updated document,
*_key* contains the key which uniquely identifies a document in a
given collection, and the attribute *_rev* contains the new document
revision. In case of an error or violated precondition, an error
object with the attribute *error* set to *true* and the attribute
*errorCode* set to the error code is built.

If the query parameter *returnOld* is *true*, then, for each
generated document, the complete previous revision of the document
is returned under the *old* attribute in the result.

If the query parameter *returnNew* is *true*, then, for each
generated document, the complete new document is returned under
the *new* attribute in the result.

Note that if any precondition is violated or an error occurred with
some of the documents, the return code is still 201 or 202, but
the additional HTTP header *X-Arango-Error-Codes* is set, which
contains a map of the error codes that occurred together with their
multiplicities, as in: *1200:17,1205:10* which means that in 17
cases the error 1200 "revision conflict" and in 10 cases the error
1205 "illegal document handle" has happened.

@RESTRETURNCODES

@RESTRETURNCODE{201}
is returned if *waitForSync* was *true* and operations were processed.

@RESTRETURNCODE{202}
is returned if *waitForSync* was *false* and operations were processed.

@RESTRETURNCODE{400}
is returned if the body does not contain a valid JSON representation
of an array of documents. The response body contains
an error document in this case.

@RESTRETURNCODE{404}
is returned if the collection was not found.

@endDocuBlock
