

@brief truncates a collection
`db._truncate(collection)`

Truncates a *collection*, removing all documents but keeping all its
indexes.

`db._truncate(collection-identifier)`

Truncates a collection identified by *collection-identified*. No error is
thrown if there is no such collection.

`db._truncate(collection-name)`

Truncates a collection named *collection-name*. No error is thrown if
there is no such collection.

@EXAMPLES

Truncates a collection:

@EXAMPLE_ARANGOSH_OUTPUT{collectionDatabaseTruncate}
~ db._create("example");
  col = db.example;
  col.save({ "Hello" : "World" });
  col.count();
  db._truncate(col);
  col.count();
~ db._drop("example");
@END_EXAMPLE_ARANGOSH_OUTPUT

Truncates a collection identified by name:

@EXAMPLE_ARANGOSH_OUTPUT{collectionDatabaseTruncateName}
~ db._create("example");
  col = db.example;
  col.save({ "Hello" : "World" });
  col.count();
  db._truncate("example");
  col.count();
~ db._drop("example");
@END_EXAMPLE_ARANGOSH_OUTPUT


