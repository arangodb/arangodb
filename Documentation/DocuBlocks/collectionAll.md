

@brief constructs an all query for a collection
`collection.all()`

Fetches all documents from a collection and returns a cursor. You can use
*toArray*, *next*, or *hasNext* to access the result. The result
can be limited using the *skip* and *limit* operator.

@EXAMPLES

Use *toArray* to get all documents at once:

@EXAMPLE_ARANGOSH_OUTPUT{001_collectionAll}
~ db._create("five");
  db.five.save({ name : "one" });
  db.five.save({ name : "two" });
  db.five.save({ name : "three" });
  db.five.save({ name : "four" });
  db.five.save({ name : "five" });
  db.five.all().toArray();
~ db._drop("five");
@END_EXAMPLE_ARANGOSH_OUTPUT

Use *limit* to restrict the documents:

@EXAMPLE_ARANGOSH_OUTPUT{002_collectionAllNext}
~ db._create("five");
  db.five.save({ name : "one" });
  db.five.save({ name : "two" });
  db.five.save({ name : "three" });
  db.five.save({ name : "four" });
  db.five.save({ name : "five" });
  db.five.all().limit(2).toArray();
~ db._drop("five");
@END_EXAMPLE_ARANGOSH_OUTPUT


