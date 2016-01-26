

@brief selects a collection from the vocbase
`db.collection-name`

Returns the collection with the given *collection-name*. If no such
collection exists, create a collection named *collection-name* with the
default properties.

@EXAMPLES

@EXAMPLE_ARANGOSH_OUTPUT{collectionDatabaseCollectionName}
~ db._create("example");
  db.example;
~ db._drop("example");
@END_EXAMPLE_ARANGOSH_OUTPUT


