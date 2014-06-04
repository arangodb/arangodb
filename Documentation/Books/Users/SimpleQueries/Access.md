!CHAPTER Sequential Access and Cursors

`cursor.hasNext()`

The hasNext operator returns true, then the cursor still has documents. In this case the next document can be accessed using the next operator, which will advance the cursor.

*Examples*

  arango> var a = db.five.all();
  arango> while (a.hasNext()) print(a.next());
  { "_id" : "five/1798296", "_key" : "1798296", "_rev" : "1798296", "doc" : 3 }
  { "_id" : "five/1732760", "_key" : "1732760", "_rev" : "1732760", "doc" : 2 }
  { "_id" : "five/1863832", "_key" : "1863832", "_rev" : "1863832", "doc" : 4 }
  { "_id" : "five/1667224", "_key" : "1667224", "_rev" : "1667224", "doc" : 1 }
  { "_id" : "five/1929368", "_key" : "1929368", "_rev" : "1929368", "doc" : 5 }

`cursor.next()`

If the hasNext operator returns true, then the underlying cursor of the simple query still has documents. In this case the next document can be accessed using the next operator, which will advance the underlying cursor. If you use next on an exhausted cursor, then undefined is returned.

*Examples*

  arango> db.five.all().next();
  { "_id" : "five/1798296", "_key" : "1798296", "_rev" : "1798296", "doc" : 3 }

cursor.setBatchSize( number)
Sets the batch size for queries. The batch size determines how many results are at most transferred from the server to the client in one chunk.


cursor.getBatchSize()
Returns the batch size for queries. If the returned value is undefined, the server will determine a sensible batch size for any following requests.


query.execute( batchSize)
Executes a simple query. If the optional batchSize value is specified, the server will return at most batchSize values in one roundtrip. The batchSize cannot be adjusted after the query is first executed.

Note that there is no need to explicitly call the execute method if another means of fetching the query results is chosen. The following two approaches lead to the same result:

result = db.users.all().toArray();
q = db.users.all(); q.execute(); result = [ ]; while (q.hasNext()) { result.push(q.next()); }
The following two alternatives both use a batchSize and return the same result:

q = db.users.all(); q.setBatchSize(20); q.execute(); while (q.hasNext()) { print(q.next()); }
q = db.users.all(); q.execute(20); while (q.hasNext()) { print(q.next()); }

cursor.dispose()
If you are no longer interested in any further results, you should call dispose in order to free any resources associated with the cursor. After calling dispose you can no longer access the cursor.


cursor.count()
The count operator counts the number of document in the result set and returns that number. The count operator ignores any limits and returns the total number of documents found.

Note
Not all simple queries support counting. In this case null is returned.
cursor.count(true)
If the result set was limited by the limit operator or documents were skiped using the skip operator, the count operator with argument true will use the number of elements in the final result set - after applying limit and skip.

Note
Not all simple queries support counting. In this case null is returned.
Examples

Ignore any limit:

  arango> db.five.all().limit(2).count();
  5
Counting any limit or skip:

  arango> db.five.all().limit(2).count(true);
  2

<!--
@anchor SimpleQueryHasNext
@copydetails JSF_SimpleQuery_prototype_hasNext

@CLEARPAGE
@anchor SimpleQueryNext
@copydetails JSF_SimpleQuery_prototype_next

@CLEARPAGE
@anchor SimpleQuerySetBatchSize
@copydetails JSF_SimpleQuery_prototype_setBatchSize

@CLEARPAGE
@anchor SimpleQueryGetBatchSize
@copydetails JSF_SimpleQuery_prototype_getBatchSize

@CLEARPAGE
@anchor SimpleQueryExecute
@copydetails JSF_SimpleQuery_prototype_execute

@CLEARPAGE
@anchor SimpleQueryDispose
@copydetails JSF_SimpleQuery_prototype_dispose

@CLEARPAGE
@anchor SimpleQueryCount
@copydetails JSF_SimpleQuery_prototype_count
-->