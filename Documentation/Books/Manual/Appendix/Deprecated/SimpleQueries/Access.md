Sequential Access and Cursors
=============================

If a query returns a cursor, then you can use *hasNext* and *next* to
iterate over the result set or *toArray* to convert it to an array.

If the number of query results is expected to be big, it is possible to 
limit the amount of documents transferred between the server and the client
to a specific value. This value is called *batchSize*. The *batchSize*
can optionally be set before or when a simple query is executed.
If the server has more documents than should be returned in a single batch,
the server will set the *hasMore* attribute in the result. It will also
return the id of the server-side cursor in the *id* attribute in the result.
This id can be used with the cursor API to fetch any outstanding results from
the server and dispose the server-side cursor afterwards.

The initial *batchSize* value can be set using the *setBatchSize*
method that is available for each type of simple query, or when the simple
query is executed using its *execute* method. If no *batchSize* value
is specified, the server will pick a reasonable default value.

### Has Next
<!-- js/common/modules/@arangodb/simple-query-common.js -->


checks if the cursor is exhausted
`cursor.hasNext()`

The *hasNext* operator returns *true*, then the cursor still has
documents. In this case the next document can be accessed using the
*next* operator, which will advance the cursor.


**Examples**


    @startDocuBlockInline cursorHasNext
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
    @endDocuBlock cursorHasNext


### Next
<!-- js/common/modules/@arangodb/simple-query-common.js -->


returns the next result document
`cursor.next()`

If the *hasNext* operator returns *true*, then the underlying
cursor of the simple query still has documents.  In this case the
next document can be accessed using the *next* operator, which
will advance the underlying cursor. If you use *next* on an
exhausted cursor, then *undefined* is returned.


**Examples**


    @startDocuBlockInline cursorNext
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
    @endDocuBlock cursorNext


### Set Batch size
<!-- js/common/modules/@arangodb/simple-query-common.js -->


sets the batch size for any following requests
`cursor.setBatchSize(number)`

Sets the batch size for queries. The batch size determines how many results
are at most transferred from the server to the client in one chunk.


### Get Batch size
<!-- js/common/modules/@arangodb/simple-query-common.js -->


returns the batch size
`cursor.getBatchSize()`

Returns the batch size for queries. If the returned value is undefined, the
server will determine a sensible batch size for any following requests.


### Execute Query
<!-- js/common/modules/@arangodb/simple-query-common.js -->


executes a query
`query.execute(batchSize)`

Executes a simple query. If the optional batchSize value is specified,
the server will return at most batchSize values in one roundtrip.
The batchSize cannot be adjusted after the query is first executed.

**Note**: There is no need to explicitly call the execute method if another
means of fetching the query results is chosen. The following two approaches
lead to the same result:

    @startDocuBlockInline executeQueryNoBatchSize
    @EXAMPLE_ARANGOSH_OUTPUT{executeQueryNoBatchSize}
    ~ db._create("users");
    ~ db.users.save({ name: "Gerhard" });
    ~ db.users.save({ name: "Helmut" });
    ~ db.users.save({ name: "Angela" });
      result = db.users.all().toArray();
      q = db.users.all(); q.execute(); result = [ ]; while (q.hasNext()) { result.push(q.next()); }
    ~ db._drop("users")
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock executeQueryNoBatchSize

The following two alternatives both use a batchSize and return the same
result:

    @startDocuBlockInline executeQueryBatchSize
    @EXAMPLE_ARANGOSH_OUTPUT{executeQueryBatchSize}
    ~ db._create("users");
    ~ db.users.save({ name: "Gerhard" });
    ~ db.users.save({ name: "Helmut" });
    ~ db.users.save({ name: "Angela" });
      q = db.users.all(); q.setBatchSize(20); q.execute(); while (q.hasNext()) { print(q.next()); }
      q = db.users.all(); q.execute(20); while (q.hasNext()) { print(q.next()); }
    ~ db._drop("users")
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock executeQueryBatchSize



### Dispose
<!-- js/common/modules/@arangodb/simple-query-common.js -->


disposes the result
`cursor.dispose()`

If you are no longer interested in any further results, you should call
*dispose* in order to free any resources associated with the cursor.
After calling *dispose* you can no longer access the cursor.


### Count
<!-- js/common/modules/@arangodb/simple-query-common.js -->


counts the number of documents
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


**Examples**


Ignore any limit:

    @startDocuBlockInline cursorCountUnLimited
    @EXAMPLE_ARANGOSH_OUTPUT{cursorCountUnLimited}
    ~ db._create("five");
    ~ db.five.save({ name : "one" });
    ~ db.five.save({ name : "two" });
    ~ db.five.save({ name : "three" });
    ~ db.five.save({ name : "four" });
    ~ db.five.save({ name : "five" });
      db.five.all().limit(2).count();
    ~ db._drop("five")
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock cursorCountUnLimited

Counting any limit or skip:

    @startDocuBlockInline cursorCountLimit
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
    @endDocuBlock cursorCountLimit


