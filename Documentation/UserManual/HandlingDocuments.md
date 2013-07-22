Handling Documents {#HandlingDocuments}
=======================================

@NAVIGATE_HandlingDocuments
@EMBEDTOC{HandlingDocumentsTOC}

Documents, Identifiers, Handles {#HandlingDocumentsIntro}
=========================================================

This is an introduction to ArangoDB's interface for documents and how handle
documents from the JavaScript shell _arangosh_. For other languages see the
corresponding language API.

@copydoc GlossaryDocument

For example:

@EXAMPLE_ARANGOSH_OUTPUT{HandlingDocumentsExample1}
    db.demo.document("demo/schlonz")
@END_EXAMPLE_ARANGOSH_OUTPUT

All documents contain special attributes: the document handle in `_id`, the
document's unique key in `_key` and and the etag aka document revision in
`_rev`. The value of the `_key` attribute can be specified by the user when
creating a document.  `_id` and `_key` values are immutable once the document
has been created. The `_rev` value is maintained by ArangoDB autonomously.

@copydoc GlossaryDocumentHandle

@copydoc GlossaryDocumentRevision

@copydoc GlossaryDocumentEtag

Address and ETag of a Document {#HandlingDocumentsResource}
===========================================================

All documents in ArangoDB have a document handle. This handle uniquely defines a
document and is managed by ArangoDB. The interface allows you to access the
documents of a collection as:

    db.@FA{collection}.document(@FA{document-handle})

For example: Assume that the document handle, which is stored in the `_id` field
of the document, is `demo/362549` and the document lives in a collection
named @FA{demo}, then that document can be accessed as:

    db.demo.document("demo/362549736")

Because the document handle is unique within the database, you
can leave out the @FA{collection} and use the shortcut:

    db._document("demo/362549736")

Each document also has a document revision or etag with is returned in the
`_rev` field when requesting a document. The document's key is returned in the
`_key` attribute.

@CLEARPAGE
Working with Documents {#HandlingDocumentsShell}
================================================

Collection Methods {#HandlingDocumentsCollectionMethods}
--------------------------------------------------------

@anchor HandlingDocumentsRead
@copydetails JS_DocumentVocbaseCol

@CLEARPAGE
@anchor HandlingDocumentsExists
@copydetails JS_ExistsVocbaseCol

@CLEARPAGE
@anchor HandlingDocumentsAny
@copydetails JS_AnyQuery

@CLEARPAGE
@anchor HandlingDocumentsCreate
@copydetails JS_SaveVocbaseCol

@CLEARPAGE
@anchor HandlingDocumentsReplace
@copydetails JS_ReplaceVocbaseCol

@CLEARPAGE
@anchor HandlingDocumentsUpdate
@copydetails JS_UpdateVocbaseCol

@CLEARPAGE
@anchor HandlingDocumentsRemove
@copydetails JS_RemoveVocbaseCol

@CLEARPAGE
@anchor HandlingDocumentsRemoveByExample
@copydetails JSF_ArangoCollection_prototype_removeByExample

@CLEARPAGE
@anchor HandlingDocumentsReplaceByExample
@copydetails JSF_ArangoCollection_prototype_replaceByExample

@CLEARPAGE
@anchor HandlingDocumentsUpdateByExample
@copydetails JSF_ArangoCollection_prototype_updateByExample

@CLEARPAGE
@anchor HandlingDocumentsFirst
@copydetails JS_FirstQuery

@CLEARPAGE
@anchor HandlingDocumentsLast
@copydetails JS_LastQuery

@CLEARPAGE
Database Methods {#HandlingDocumentsDatabaseMethods}
----------------------------------------------------

@anchor HandlingDocumentsDbRead
@copydetails JS_DocumentVocbase

@CLEARPAGE
@anchor HandlingDocumentsDbExists
@copydetails JS_ExistsVocbase

@CLEARPAGE
@anchor HandlingDocumentsDbReplace
@copydetails JS_ReplaceVocbase

@CLEARPAGE
@anchor HandlingDocumentsDbUpdate
@copydetails JS_UpdateVocbase

@CLEARPAGE
@anchor HandlingDocumentsDbRemove
@copydetails JS_RemoveVocbase
