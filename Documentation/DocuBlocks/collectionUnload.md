

@brief unloads a collection
`collection.unload()`

Starts unloading a collection from memory. Note that unloading is deferred
until all query have finished.

@EXAMPLES

@EXAMPLE_ARANGOSH_OUTPUT{CollectionUnload}
~ db._create("example");
  col = db.example;
  col.unload();
  col;
~ db._drop("example");
@END_EXAMPLE_ARANGOSH_OUTPUT


