!CHAPTER Sequential Access and Cursors

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

!SUBSECTION Has Next
<!-- js/common/modules/org/arangodb/simple-query-common.js -->
@startDocuBlock cursorHasNext

!SUBSECTION Next
<!-- js/common/modules/org/arangodb/simple-query-common.js -->
@startDocuBlock cursorNext

!SUBSECTION Set Batch size
<!-- js/common/modules/org/arangodb/simple-query-common.js -->
@startDocuBlock cursorSetBatchSize

!SUBSECTION Get Batch size
<!-- js/common/modules/org/arangodb/simple-query-common.js -->
@startDocuBlock cursorGetBatchSize

!SUBSECTION Execute Query
<!-- js/common/modules/org/arangodb/simple-query-common.js -->
@startDocuBlock queryExecute

!SUBSECTION Dispose
<!-- js/common/modules/org/arangodb/simple-query-common.js -->
@startDocuBlock cursorDispose

!SUBSECTION Count
<!-- js/common/modules/org/arangodb/simple-query-common.js -->
@startDocuBlock cursorCount