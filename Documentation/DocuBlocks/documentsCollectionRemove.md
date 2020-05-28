

@brief removes a document
`collection.remove(document)`

Removes a document. If there is revision mismatch, then an error is thrown.

`collection.remove(document, true)`

Removes a document. If there is revision mismatch, then mismatch is ignored
and document is deleted. The function returns *true* if the document
existed and was deleted. It returns *false*, if the document was already
deleted.

`collection.remove(document, true, waitForSync)`

The optional *waitForSync* parameter can be used to force synchronization
of the document deletion operation to disk even in case that the
*waitForSync* flag had been disabled for the entire collection.  Thus,
the *waitForSync* parameter can be used to force synchronization of just
specific operations. To use this, set the *waitForSync* parameter to
*true*. If the *waitForSync* parameter is not specified or set to
*false*, then the collection's default *waitForSync* behavior is
applied. The *waitForSync* parameter cannot be used to disable
synchronization for collections that have a default *waitForSync* value
of *true*.

`collection.remove(document-id, data)`

As before. Instead of document a *document-id* can be passed as
first argument.

@EXAMPLES

Remove a document:

@EXAMPLE_ARANGOSH_OUTPUT{documentDocumentRemove}
~ db._create("example");
  a1 = db.example.insert({ a : 1 });
  db.example.document(a1);
  db.example.remove(a1);
  db.example.document(a1); // xpError(ERROR_ARANGO_DOCUMENT_NOT_FOUND);
~ db._drop("example");
@END_EXAMPLE_ARANGOSH_OUTPUT

Remove a document with a conflict:

@EXAMPLE_ARANGOSH_OUTPUT{documentDocumentRemoveConflict}
~ db._create("example");
  a1 = db.example.insert({ a : 1 });
  a2 = db.example.replace(a1, { a : 2 });
  db.example.remove(a1);       // xpError(ERROR_ARANGO_CONFLICT);
  db.example.remove(a1, true);
  db.example.document(a1);     // xpError(ERROR_ARANGO_DOCUMENT_NOT_FOUND);
~ db._drop("example");
@END_EXAMPLE_ARANGOSH_OUTPUT


