

@brief constructs a query-by-example for a collection
`collection.firstExample(example)`

Returns the first document of a collection that matches the specified
example. If no such document exists, *null* will be returned. 
The example has to be specified as paths and values.
See *byExample* for details.

`collection.firstExample(path1, value1, ...)`

As alternative you can supply an array of paths and values.

@EXAMPLES

@EXAMPLE_ARANGOSH_OUTPUT{collectionFirstExample}
~ db._create("users");
~ db.users.save({ name: "Gerhard" });
~ db.users.save({ name: "Helmut" });
~ db.users.save({ name: "Angela" });
  db.users.firstExample("name", "Angela");
~ db._drop("users");
@END_EXAMPLE_ARANGOSH_OUTPUT


