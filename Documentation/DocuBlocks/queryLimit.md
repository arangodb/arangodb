

@brief limit
`query.limit(number)`

Limits a result to the first *number* documents. Specifying a limit of
*0* will return no documents at all. If you do not need a limit, just do
not add the limit operator. The limit must be non-negative.

In general the input to *limit* should be sorted. Otherwise it will be
unclear which documents will be included in the result set.

@EXAMPLES

@EXAMPLE_ARANGOSH_OUTPUT{queryLimit}
~ db._create("five");
~ db.five.save({ name : "one" });
~ db.five.save({ name : "two" });
~ db.five.save({ name : "three" });
~ db.five.save({ name : "four" });
~ db.five.save({ name : "five" });
  db.five.all().toArray();
  db.five.all().limit(2).toArray();
~ db._drop("five")
@END_EXAMPLE_ARANGOSH_OUTPUT


