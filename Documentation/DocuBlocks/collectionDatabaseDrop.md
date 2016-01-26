

@brief drops a collection
`db._drop(collection)`

Drops a *collection* and all its indexes.

`db._drop(collection-identifier)`

Drops a collection identified by *collection-identifier* and all its
indexes. No error is thrown if there is no such collection.

`db._drop(collection-name)`

Drops a collection named *collection-name* and all its indexes. No error
is thrown if there is no such collection.

*Examples*

Drops a collection:

@EXAMPLE_ARANGOSH_OUTPUT{collectionDatabaseDrop}
~ db._create("example");
  col = db.example;
  db._drop(col);
  col;
~ db._drop("example");
@END_EXAMPLE_ARANGOSH_OUTPUT

Drops a collection identified by name:

@EXAMPLE_ARANGOSH_OUTPUT{collectionDatabaseDropName}
~ db._create("example");
  col = db.example;
  db._drop("example");
  col;
@END_EXAMPLE_ARANGOSH_OUTPUT


