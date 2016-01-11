

@brief skip
`query.skip(number)`

Skips the first *number* documents. If *number* is positive, then this
number of documents are skipped before returning the query results.

In general the input to *skip* should be sorted. Otherwise it will be
unclear which documents will be included in the result set.

Note: using negative *skip* values is **deprecated** as of ArangoDB 2.6 and 
will not be supported in future versions of ArangoDB.

@EXAMPLES

@EXAMPLE_ARANGOSH_OUTPUT{querySkip}
~ db._create("five");
~ db.five.save({ name : "one" });
~ db.five.save({ name : "two" });
~ db.five.save({ name : "three" });
~ db.five.save({ name : "four" });
~ db.five.save({ name : "five" });
  db.five.all().toArray();
  db.five.all().skip(3).toArray();
~ db._drop("five")
@END_EXAMPLE_ARANGOSH_OUTPUT


