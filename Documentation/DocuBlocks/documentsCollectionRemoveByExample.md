

@brief removes documents matching an example
`collection.removeByExample(example)`

Removes all documents matching an example.

`collection.removeByExample(document, waitForSync)`

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

`collection.removeByExample(document, waitForSync, limit)`

The optional *limit* parameter can be used to restrict the number of
removals to the specified value. If *limit* is specified but less than the
number of documents in the collection, it is undefined which documents are
removed.

@EXAMPLES

@EXAMPLE_ARANGOSH_OUTPUT{010_documentsCollectionRemoveByExample}
~ db._create("example");
~ db.example.save({ Hello : "world" });
  db.example.removeByExample( {Hello : "world"} );
~ db._drop("example");
@END_EXAMPLE_ARANGOSH_OUTPUT

