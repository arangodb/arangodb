

@brief replaces a document
`collection.replace(document, data)`

Replaces an existing *document*. The *document* must be a document in
the current collection. This document is then replaced with the
*data* given as second argument.

The method returns a document with the attributes *_id*, *_rev* and
*{_oldRev*.  The attribute *_id* contains the document identifier of the
updated document, the attribute *_rev* contains the document revision of
the updated document, the attribute *_oldRev* contains the revision of
the old (now replaced) document.

If there is a conflict, i. e. if the revision of the *document* does not
match the revision in the collection, then an error is thrown.

`collection.replace(document, data, true)` or
`collection.replace(document, data, overwrite: true)`

As before, but in case of a conflict, the conflict is ignored and the old
document is overwritten.

`collection.replace(document, data, true, waitForSync)` or
`collection.replace(document, data, overwrite: true, waitForSync: true or false)`

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

`collection.replace(document-id, data)`

As before. Instead of document a *document-id* can be passed as
first argument.

@EXAMPLES

Create and update a document:

@EXAMPLE_ARANGOSH_OUTPUT{documentsCollectionReplace}
~ db._create("example");
  a1 = db.example.insert({ a : 1 });
  a2 = db.example.replace(a1, { a : 2 });
  a3 = db.example.replace(a1, { a : 3 }); // xpError(ERROR_ARANGO_CONFLICT);
~ db._drop("example");
@END_EXAMPLE_ARANGOSH_OUTPUT

Use a document identifier:

@EXAMPLE_ARANGOSH_OUTPUT{documentsCollectionReplaceHandle}
~ db._create("example");
~ var myid = db.example.insert({_key: "3903044"});
  a1 = db.example.insert({ a : 1 });
  a2 = db.example.replace("example/3903044", { a : 2 });
~ db._drop("example");
@END_EXAMPLE_ARANGOSH_OUTPUT


