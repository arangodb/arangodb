

@brief returns the next result document
`cursor.next()`

If the *hasNext* operator returns *true*, then the underlying
cursor of the simple query still has documents.  In this case the
next document can be accessed using the *next* operator, which
will advance the underlying cursor. If you use *next* on an
exhausted cursor, then *undefined* is returned.

@EXAMPLES

@EXAMPLE_ARANGOSH_OUTPUT{cursorNext}
~ db._create("five");
~ db.five.save({ name : "one" });
~ db.five.save({ name : "two" });
~ db.five.save({ name : "three" });
~ db.five.save({ name : "four" });
~ db.five.save({ name : "five" });
  db.five.all().next();
~ db._drop("five")
@END_EXAMPLE_ARANGOSH_OUTPUT

