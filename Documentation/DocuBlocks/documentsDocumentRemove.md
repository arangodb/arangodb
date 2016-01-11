////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock documentsDocumentRemove
/// @brief removes a document
/// `db._remove(document)`
///
/// Removes a document. If there is revision mismatch, then an error is thrown.
///
/// `db._remove(document, true)`
///
/// Removes a document. If there is revision mismatch, then mismatch is ignored
/// and document is deleted. The function returns *true* if the document
/// existed and was deleted. It returns *false*, if the document was already
/// deleted.
///
/// `db._remove(document, true, waitForSync)` or
/// `db._remove(document, {overwrite: true or false, waitForSync: true or false})`
///
/// The optional *waitForSync* parameter can be used to force synchronization
/// of the document deletion operation to disk even in case that the
/// *waitForSync* flag had been disabled for the entire collection.  Thus,
/// the *waitForSync* parameter can be used to force synchronization of just
/// specific operations. To use this, set the *waitForSync* parameter to
/// *true*. If the *waitForSync* parameter is not specified or set to
/// *false*, then the collection's default *waitForSync* behavior is
/// applied. The *waitForSync* parameter cannot be used to disable
/// synchronization for collections that have a default *waitForSync* value
/// of *true*.
///
/// `db._remove(document-handle, data)`
///
/// As before. Instead of document a *document-handle* can be passed as first
/// argument.
///
/// @EXAMPLES
///
/// Remove a document:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{documentsCollectionRemove}
/// ~ db._create("example");
///   a1 = db.example.insert({ a : 1 });
///   db._remove(a1);
///   db._remove(a1);  // xpError(ERROR_ARANGO_DOCUMENT_NOT_FOUND);
///   db._remove(a1, true);
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Remove a document with a conflict:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{documentsCollectionRemoveConflict}
/// ~ db._create("example");
///   a1 = db.example.insert({ a : 1 });
///   a2 = db._replace(a1, { a : 2 });
///   db._remove(a1);       // xpError(ERROR_ARANGO_CONFLICT)
///   db._remove(a1, true);
///   db._document(a1);     // xpError(ERROR_ARANGO_DOCUMENT_NOT_FOUND)
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Remove a document using new signature:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{documentsCollectionRemoveSignature}
/// ~ db._create("example");
///   db.example.insert({ a:  1 } );
/// | db.example.remove("example/11265325374",
///        { overwrite: true, waitForSync: false})
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////