Handling Documents {#ShellDocument}
===================================

@NAVIGATE_ShellDocument
@EMBEDTOC{ShellDocumentTOC}

This is an introduction to ArangoDB's interface for documents and how handle
documents from the JavaScript shell _arangosh_. For other languages see
the corresponding language API.

Documents, Identifiers, Handles {#ShellDocumentIntro}
=====================================================

@copydoc GlossaryDocument

For example:

    {
      "_id" : "1234567/2345678",
      "_rev" : "3456789",
      "firstName" : "Hugo",
      "lastName" : "Schlonz",
      "address" : {
	"street" : "Strasse 1",
	"city" : "Hier"
      },
      "hobbies" : [
	"swimming",
	"biking",
	"programming"
      ]
    }

All documents contain two special fields, the document handle in `_id`
and the etag aka document revision in `_rev`.

@copydoc GlossaryDocumentHandle

@copydoc GlossaryDocumentIdentifier

@copydoc GlossaryDocumentRevision

@copydoc GlossaryDocumentEtag

Address and ETag of a Document {#ShellDocumentResource}
========================================================

All documents in ArangoDB have a document handle. This handle uniquely
defines a document and is managed by ArangoDB. The interface allows
you to access the documents of a collection as:

    db.@FA{collection}.documet(@FA{document-handle})

For example: Assume that the document handle, which is stored in
the `_id` field of the document, is `7254820/362549`
and the document lives in a collection named @FA{demo}, then
that document can be accessed as:

    db.demo.document("7254820/362549736")

Because the document handle is unique within the database, you
can leave out the @FA{collection} and use the shortcut:

    db._document("7254820/362549736")

Each document also has a document revision or etag with is returned
in the `_rev` field when requesting a document.

@CLEARPAGE
Working with Documents {#ShellDocumentShell}
============================================

Collection Methods {#ShellDocumentCollectionMethods}
----------------------------------------------------

@anchor ShellDocumentRead
@copydetails JS_DocumentVocbaseCol

@anchor ShellDocumentAny
@copydetails JS_AnyQuery

@CLEARPAGE
@anchor ShellDocumentCreate
@copydetails JS_SaveVocbaseCol

@CLEARPAGE
@anchor ShellDocumentReplace
@copydetails JS_ReplaceVocbaseCol

@CLEARPAGE
@anchor ShellDocumentUpdate
@copydetails JS_UpdateVocbaseCol

@CLEARPAGE
@anchor ShellDocumentRemove
@copydetails JS_RemoveVocbaseCol

@CLEARPAGE
@anchor ShellDocumentRemoveByExample
@copydetails JSF_ArangoCollection_prototype_removeByExample

@CLEARPAGE
Database Methods {#ShellDocumentDatabaseMethods}
------------------------------------------------

@anchor ShellDocumentDbRead
@copydetails JS_DocumentVocbase

@CLEARPAGE
@anchor ShellDocumentDbReplace
@copydetails JS_ReplaceVocbase

@CLEARPAGE
@anchor ShellDocumentDbUpdate
@copydetails JS_UpdateVocbase

@CLEARPAGE
@anchor ShellDocumentDbRemove
@copydetails JS_RemoveVocbase
