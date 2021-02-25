

@brief update a document
`db._update(document, data, overwrite, keepNull, waitForSync)`

Updates an existing *document*. The *document* must be a document in
the current collection. This document is then patched with the
*data* given as second argument. The optional *overwrite* parameter can
be used to control the behavior in case of version conflicts (see below).
The optional *keepNull* parameter can be used to modify the behavior when
handling *null* values. Normally, *null* values are stored in the
database. By setting the *keepNull* parameter to *false*, this behavior
can be changed so that all attributes in *data* with *null* values will
be removed from the target document.

The optional *waitForSync* parameter can be used to force
synchronization of the document update operation to disk even in case
that the *waitForSync* flag had been disabled for the entire collection.
Thus, the *waitForSync* parameter can be used to force synchronization
of just specific operations. To use this, set the *waitForSync* parameter
to *true*. If the *waitForSync* parameter is not specified or set to
false*, then the collection's default *waitForSync* behavior is
applied. The *waitForSync* parameter cannot be used to disable
synchronization for collections that have a default *waitForSync* value
of *true*.

The method returns a document with the attributes *_id*, *_rev* and
*_oldRev*. The attribute *_id* contains the document identifier of the
updated document, the attribute *_rev* contains the document revision of
the updated document, the attribute *_oldRev* contains the revision of
the old (now replaced) document.

If there is a conflict, i. e. if the revision of the *document* does not
match the revision in the collection, then an error is thrown.

`db._update(document, data, true)`

As before, but in case of a conflict, the conflict is ignored and the old
document is overwritten.

`db._update(document-id, data)`

As before. Instead of document a *document-id* can be passed as
first argument.

@EXAMPLES

Create and update a document:

@EXAMPLE_ARANGOSH_OUTPUT{documentDocumentUpdate}
~ db._create("example");
  a1 = db.example.insert({ a : 1 });
  a2 = db._update(a1, { b : 2 });
  a3 = db._update(a1, { c : 3 }); // xpError(ERROR_ARANGO_CONFLICT);
~ db._drop("example");
@END_EXAMPLE_ARANGOSH_OUTPUT


