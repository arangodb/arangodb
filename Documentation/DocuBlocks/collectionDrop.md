

@brief drops a collection
`collection.drop()`

Drops a *collection* and all its indexes.

@EXAMPLES

@EXAMPLE_ARANGOSH_OUTPUT{collectionDrop}
~ db._create("example");
  col = db.example;
  col.drop();
  col;
~ db._drop("example");
@END_EXAMPLE_ARANGOSH_OUTPUT


