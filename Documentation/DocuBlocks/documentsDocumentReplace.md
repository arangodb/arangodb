

@brief replaces a document
`db._replace(document, data)`

The method returns a document with the attributes *_id*, *_rev* and
*_oldRev*.  The attribute *_id* contains the document identifier of the
updated document, the attribute *_rev* contains the document revision of
the updated document, the attribute *_oldRev* contains the revision of
the old (now replaced) document.

If there is a conflict, i. e. if the revision of the *document* does not
match the revision in the collection, then an error is thrown.

`db._replace(document, data, true)`

As before, but in case of a conflict, the conflict is ignored and the old
document is overwritten.

`db._replace(document, data, true, waitForSync)`

The optional *waitForSync* parameter can be used to force
synchronization of the document replacement operation to disk even in case
that the *waitForSync* flag had been disabled for the entire collection.
Thus, the *waitForSync* parameter can be used to force synchronization
of just specific operations. To use this, set the *waitForSync* parameter
to *true*. If the *waitForSync* parameter is not specified or set to
*false*, then the collection's default *waitForSync* behavior is
applied. The *waitForSync* parameter cannot be used to disable
synchronization for collections that have a default *waitForSync* value
of *true*.

`db._replace(document-id, data)`

As before. Instead of document a *document-id* can be passed as
first argument.

@EXAMPLES

Create and replace a document:

@EXAMPLE_ARANGOSH_OUTPUT{documentsDocumentReplace}
~ db._create("example");
  a1 = db.example.insert({ a : 1 });
  a2 = db._replace(a1, { a : 2 });
  a3 = db._replace(a1, { a : 3 });  // xpError(ERROR_ARANGO_CONFLICT);
~ db._drop("example");
@END_EXAMPLE_ARANGOSH_OUTPUT


