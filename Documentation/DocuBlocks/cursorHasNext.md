

@brief checks if the cursor is exhausted
`cursor.hasNext()`

The *hasNext* operator returns *true*, then the cursor still has
documents. In this case the next document can be accessed using the
*next* operator, which will advance the cursor.

@EXAMPLES

@EXAMPLE_ARANGOSH_OUTPUT{cursorHasNext}
~ db._create("five");
~ db.five.save({ name : "one" });
~ db.five.save({ name : "two" });
~ db.five.save({ name : "three" });
~ db.five.save({ name : "four" });
~ db.five.save({ name : "five" });
  var a = db.five.all();
  while (a.hasNext()) print(a.next());
~ db._drop("five")
@END_EXAMPLE_ARANGOSH_OUTPUT

