

@brief returns all collections
`db._collections()`

Returns all collections of the given database.

@EXAMPLES

@EXAMPLE_ARANGOSH_OUTPUT{collectionsDatabaseName}
~ db._create("example");
  db._collections();
~ db._drop("example");
@END_EXAMPLE_ARANGOSH_OUTPUT


