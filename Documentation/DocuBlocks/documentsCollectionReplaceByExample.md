

@brief replaces documents matching an example
`collection.replaceByExample(example, newValue)`

Replaces all documents matching an example with a new document body.
The entire document body of each document matching the *example* will be
replaced with *newValue*. The document meta-attributes such as *_id*,
*_key*, *_from*, *_to* will not be replaced.

`collection.replaceByExample(document, newValue, waitForSync)`

The optional *waitForSync* parameter can be used to force synchronization
of the document replacement operation to disk even in case that the
*waitForSync* flag had been disabled for the entire collection.  Thus,
the *waitForSync* parameter can be used to force synchronization of just
specific operations. To use this, set the *waitForSync* parameter to
*true*. If the *waitForSync* parameter is not specified or set to
*false*, then the collection's default *waitForSync* behavior is
applied. The *waitForSync* parameter cannot be used to disable
synchronization for collections that have a default *waitForSync* value
of *true*.

`collection.replaceByExample(document, newValue, waitForSync, limit)`

The optional *limit* parameter can be used to restrict the number of
replacements to the specified value. If *limit* is specified but less than
the number of documents in the collection, it is undefined which documents are
replaced.

@EXAMPLES

@EXAMPLE_ARANGOSH_OUTPUT{011_documentsCollectionReplaceByExample}
~ db._create("example");
  db.example.save({ Hello : "world" });
  db.example.replaceByExample({ Hello: "world" }, {Hello: "mars"}, false, 5);
~ db._drop("example");
@END_EXAMPLE_ARANGOSH_OUTPUT

