Pagination
==========

If, for example, you display the result of a user search, then you are in
general not interested in the completed result set, but only the first 10 or so
documents. Or maybe the next 10 documents for the second page. In this case, you
can the *skip* and *limit* operators. These operators work like LIMIT in
MySQL.

*skip* used together with *limit* can be used to implement pagination.
The *skip* operator skips over the first n documents. So, in order to create
result pages with 10 result documents per page, you can use <i>skip(n *
10).limit(10)</i> to access the 10 documents on the *n*th page. This result
should be sorted, so that the pagination works in a predicable way.

### Limit
<!-- js/common/modules/@arangodb/simple-query-common.js -->


limit
`query.limit(number)`

Limits a result to the first *number* documents. Specifying a limit of
*0* will return no documents at all. If you do not need a limit, just do
not add the limit operator. The limit must be non-negative.

In general the input to *limit* should be sorted. Otherwise it will be
unclear which documents will be included in the result set.


**Examples**


    @startDocuBlockInline queryLimit
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
    @endDocuBlock queryLimit



### Skip
<!-- js/common/modules/@arangodb/simple-query-common.js -->


skip
`query.skip(number)`

Skips the first *number* documents. If *number* is positive, then this
number of documents are skipped before returning the query results.

In general the input to *skip* should be sorted. Otherwise it will be
unclear which documents will be included in the result set.

Note: using negative *skip* values is **deprecated** as of ArangoDB 2.6 and 
will not be supported in future versions of ArangoDB.


**Examples**


    @startDocuBlockInline querySkip
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
    @endDocuBlock querySkip


