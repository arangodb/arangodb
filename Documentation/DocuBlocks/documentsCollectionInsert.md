

@brief insert a new document
`collection.insert(data)`

Creates a new document in the *collection* from the given *data*. The
*data* must be an object.

The method returns a document with the attributes *_id* and *_rev*.
The attribute *_id* contains the document identifier of the newly created
document, the attribute *_rev* contains the document revision.

`collection.insert(data, waitForSync)`

Creates a new document in the *collection* from the given *data* as
above. The optional *waitForSync* parameter can be used to force
synchronization of the document creation operation to disk even in case
that the *waitForSync* flag had been disabled for the entire collection.
Thus, the *waitForSync* parameter can be used to force synchronization
of just specific operations. To use this, set the *waitForSync* parameter
to *true*. If the *waitForSync* parameter is not specified or set to
*false*, then the collection's default *waitForSync* behavior is
applied. The *waitForSync* parameter cannot be used to disable
synchronization for collections that have a default *waitForSync* value
of *true*.

Note: since ArangoDB 2.2, *insert* is an alias for *save*.

@EXAMPLES

@EXAMPLE_ARANGOSH_OUTPUT{documentsCollectionInsert}
~ db._create("example");
  db.example.insert({ Hello : "World" });
  db.example.insert({ Hello : "World" }, true);
~ db._drop("example");
@END_EXAMPLE_ARANGOSH_OUTPUT


