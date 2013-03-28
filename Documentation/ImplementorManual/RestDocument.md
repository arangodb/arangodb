REST Interface for Documents {#RestDocument}
============================================

@NAVIGATE_RestDocument
@EMBEDTOC{RestDocumentTOC}

Documents, Identifiers, Handles {#RestDocumentIntro}
====================================================

This is an introduction to ArangoDB's REST interface for documents.

@copydoc GlossaryDocument

For example:

@verbinclude document1

All documents contain special attributes: the document handle in `_id`, the
document's unique key in `_key` and and the etag aka document revision in
`_rev`. The value of the `_key` attribute can be specified by the user when
creating a document.  `_id` and `_key` values are immutable once the document
has been created. The `_rev` value is maintained by ArangoDB autonomously.

@copydoc GlossaryDocumentHandle

@copydoc GlossaryDocumentKey

@copydoc GlossaryDocumentRevision

@copydoc GlossaryDocumentEtag

The basic operations (create, read, update, delete) for documents are mapped to
the standard HTTP methods (`POST`, `GET`, `PUT`, `DELETE`). There is also a 
partial update method, which is mapped to the HTTP `PATCH` method.

An identifier for the document revision is returned in the `ETag` header field. 
If you modify a document, you can use the `If-Match` field to detect conflicts. 
The revision of a document can be checking using the HTTP method `HEAD`.

Address and ETag of a Document {#RestDocumentResource}
======================================================

All documents in ArangoDB have a document handle. This handle uniquely defines a
document and is managed by ArangoDB. All documents are found under the URI:

    http://server:port/_api/document/document-handle

For example: Assume that the document handle, which is stored in the `_id`
attribute of the document, is `demo/362549736`, then the URL of that document
is:

    http://localhost:8529/_api/document/demo/362549736

Each document also has a document revision or etag with is returned in the
"ETag" header field when requesting a document.

If you obtain a document using `GET` and you want to check if a newer revision
is available, then you can use the `If-None-Match` header. If the document is
unchanged, a `HTTP 412` is returned.

If you want to update or delete a document, then you can use the `If-Match`
header. If the document has changed, then the operation is aborted and a `HTTP
412` is returned.

Working with Documents using REST {#RestDocumentHttp}
=====================================================

@CLEARPAGE
@anchor RestDocumentRead
@copydetails triagens::arango::RestDocumentHandler::readSingleDocument

@CLEARPAGE
@anchor RestDocumentCreate
@copydetails triagens::arango::RestDocumentHandler::createDocument

@CLEARPAGE
@anchor RestDocumentReplace
@copydetails triagens::arango::RestDocumentHandler::replaceDocument

@CLEARPAGE
@anchor RestDocumentUpdate
@copydetails triagens::arango::RestDocumentHandler::updateDocument

@CLEARPAGE
@anchor RestDocumentDelete
@copydetails triagens::arango::RestDocumentHandler::deleteDocument

@CLEARPAGE
@anchor RestDocumentHead
@copydetails triagens::arango::RestDocumentHandler::checkDocument

@CLEARPAGE
@anchor RestDocumentReadAll
@copydetails triagens::arango::RestDocumentHandler::readAllDocuments
