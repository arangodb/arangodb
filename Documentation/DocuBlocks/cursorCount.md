

@brief counts the number of documents
`cursor.count()`

The *count* operator counts the number of document in the result set and
returns that number. The *count* operator ignores any limits and returns
the total number of documents found.

**Note**: Not all simple queries support counting. In this case *null* is
returned.

`cursor.count(true)`

If the result set was limited by the *limit* operator or documents were
skiped using the *skip* operator, the *count* operator with argument
*true* will use the number of elements in the final result set - after
applying *limit* and *skip*.

**Note**: Not all simple queries support counting. In this case *null* is
returned.

@EXAMPLES

Ignore any limit:

@EXAMPLE_ARANGOSH_OUTPUT{cursorCount}
~ db._create("five");
~ db.five.save({ name : "one" });
~ db.five.save({ name : "two" });
~ db.five.save({ name : "three" });
~ db.five.save({ name : "four" });
~ db.five.save({ name : "five" });
  db.five.all().limit(2).count();
~ db._drop("five")
@END_EXAMPLE_ARANGOSH_OUTPUT

Counting any limit or skip:

@EXAMPLE_ARANGOSH_OUTPUT{cursorCountLimit}
~ db._create("five");
~ db.five.save({ name : "one" });
~ db.five.save({ name : "two" });
~ db.five.save({ name : "three" });
~ db.five.save({ name : "four" });
~ db.five.save({ name : "five" });
  db.five.all().limit(2).count(true);
~ db._drop("five")
@END_EXAMPLE_ARANGOSH_OUTPUT


