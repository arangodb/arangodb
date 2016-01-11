

@brief truncates a collection
`collection.truncate()`

Truncates a *collection*, removing all documents but keeping all its
indexes.

@EXAMPLES

Truncates a collection:

@EXAMPLE_ARANGOSH_OUTPUT{collectionTruncate}
~ db._create("example");
  col = db.example;
  col.save({ "Hello" : "World" });
  col.count();
  col.truncate();
  col.count();
~ db._drop("example");
@END_EXAMPLE_ARANGOSH_OUTPUT

