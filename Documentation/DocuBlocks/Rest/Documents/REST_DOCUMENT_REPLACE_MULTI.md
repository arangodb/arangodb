////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock REST_DOCUMENT_REPLACE_MULTI
/// @brief replaces multiple documents
///
/// @RESTHEADER{PUT /_api/document/{collection},Replace documents}
///
/// @RESTALLBODYPARAM{documents,json,required}
/// A JSON representation of an array of documents.
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{collection,string,required}
/// This URL parameter is the name of the collection in which the
/// documents are replaced.
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{waitForSync,boolean,optional}
/// Wait until the new documents have been synced to disk.
///
/// @RESTQUERYPARAM{ignoreRevs,boolean,optional}
/// By default, or if this is set to *true*, the *_rev* attributes in 
/// the given documents are ignored. If this is set to *false*, then
/// any *_rev* attribute given in a body document is taken as a
/// precondition. The document is only replaced if the current revision
/// is the one specified.
///
/// @RESTQUERYPARAM{returnOld,boolean,optional}
/// Return additionally the complete previous revision of the changed 
/// documents under the attribute *old* in the result.
///
/// @RESTQUERYPARAM{returnNew,boolean,optional}
/// Return additionally the complete new documents under the attribute *new*
/// in the result.
///
/// @RESTDESCRIPTION
/// Replaces multiple documents in the specified collection with the
/// ones in the body, the replaced documents are specified by the *_key*
/// attributes in the body documents. The operation is only performed,
/// if no precondition is violated and the operation succeeds for all
/// documents or fails without replacing a single document.
///
/// If *ignoreRevs* is *false* and there is a *_rev* attribute in a
/// document in the body and its value does not match the revision of
/// the corresponding document in the database, the precondition is
/// violated.
///
/// If the document exists and can be updated, then an *HTTP 201* or
/// an *HTTP 202* is returned (depending on *waitForSync*, see below).
///
/// Optionally, the query parameter *waitForSync* can be used to force
/// synchronization of the document replacement operation to disk even in case
/// that the *waitForSync* flag had been disabled for the entire collection.
/// Thus, the *waitForSync* query parameter can be used to force synchronization
/// of just specific operations. To use this, set the *waitForSync* parameter
/// to *true*. If the *waitForSync* parameter is not specified or set to
/// *false*, then the collection's default *waitForSync* behavior is
/// applied. The *waitForSync* query parameter cannot be used to disable
/// synchronization for collections that have a default *waitForSync* value
/// of *true*.
///
/// The body of the response contains a JSON array with the information
/// about the handle and the revision of the replaced documents. The
/// attribute *_id* contains the known *document-handle* of each updated
/// document, *_key* contains the key which uniquely identifies a
/// document in a given collection, and the attribute *_rev* contains
/// the new document revision.
///
/// If the query parameter *returnOld* is *true*, then, for each
/// generated document, the complete previous revision of the document
/// is returned under the *old* attribute in the result.
///
/// If the query parameter *returnNew* is *true*, then, for each
/// generated document, the complete new document is returned under
/// the *new* attribute in the result.
///
/// If any of the document does not exist, then a *HTTP 404* is returned
/// and the body of the response contains an error document.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{201}
/// is returned if the documents were replaced successfully and
/// *waitForSync* was *true*.
///
/// @RESTRETURNCODE{202}
/// is returned if the documents were replaced successfully and
/// *waitForSync* was *false*.
///
/// @RESTRETURNCODE{400}
/// is returned if the body does not contain a valid JSON representation
/// of an array of documents. The response body contains
/// an error document in this case.
///
/// @RESTRETURNCODE{404}
/// is returned if the collection or a document was not found.
///
/// @RESTRETURNCODE{412}
/// is returned if the precondition was violated. The response will
/// also contain the found documents' current revisions in the *_rev*
/// attributes. Additionally, the attributes *_id* and *_key* will be
/// returned.
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
