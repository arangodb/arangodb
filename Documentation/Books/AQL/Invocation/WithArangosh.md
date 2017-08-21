Executing queries from Arangosh
===============================

Within the ArangoDB shell, the *_query* and *_createStatement* methods of the
*db* object can be used to execute AQL queries. This chapter also describes
how to use bind parameters, counting, statistics and cursors. 

### with db._query

One can execute queries with the *_query* method of the *db* object. 
This will run the specified query in the context of the currently
selected database and return the query results in a cursor. The results of the cursor
can be printed using its *toArray* method:
    
    @startDocuBlockInline 01_workWithAQL_all
    @EXAMPLE_ARANGOSH_OUTPUT{01_workWithAQL_all}
    ~addIgnoreCollection("mycollection")
    db._create("mycollection")
    db.mycollection.save({ _key: "testKey", Hello : "World" })
    db._query('FOR my IN mycollection RETURN my._key').toArray()
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 01_workWithAQL_all

#### db._query Bind parameters

To pass bind parameters into a query, they can be specified as second argument to the
*_query* method:

    @startDocuBlockInline 02_workWithAQL_bindValues
    @EXAMPLE_ARANGOSH_OUTPUT{02_workWithAQL_bindValues}
    |db._query(
    | 'FOR c IN @@collection FILTER c._key == @key RETURN c._key', {
    |   '@collection': 'mycollection', 
    |   'key': 'testKey'
    }).toArray();
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 02_workWithAQL_bindValues

#### ES6 template strings

It is also possible to use ES6 template strings for generating AQL queries. There is
a template string generator function named *aql*; we call it once to demonstrate
its result, and once putting it directly into the query:

```js
var key = 'testKey';
aql`FOR c IN mycollection FILTER c._key == ${key} RETURN c._key`;
{ 
  "query" : "FOR c IN mycollection FILTER c._key == @value0 RETURN c._key", 
  "bindVars" : { 
    "value0" : "testKey" 
  } 
}
```

    @startDocuBlockInline 02_workWithAQL_aqlQuery
    @EXAMPLE_ARANGOSH_OUTPUT{02_workWithAQL_aqlQuery}
      var key = 'testKey';
      |db._query(
      | aql`FOR c IN mycollection FILTER c._key == ${key} RETURN c._key`
        ).toArray();
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 02_workWithAQL_aqlQuery

Arbitrary JavaScript expressions can be used in queries that are generated with the 
*aql* template string generator. Collection objects are handled automatically:

    @startDocuBlockInline 02_workWithAQL_aqlCollectionQuery
    @EXAMPLE_ARANGOSH_OUTPUT{02_workWithAQL_aqlCollectionQuery}
      var key = 'testKey';
      |db._query(aql`FOR doc IN ${ db.mycollection } RETURN doc`
          ).toArray();
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 02_workWithAQL_aqlCollectionQuery

Note: data-modification AQL queries normally do not return a result (unless the AQL query 
contains an extra *RETURN* statement). When not using a *RETURN* statement in the query, the 
*toArray* method will return an empty array.

#### Statistics and extra Information
 
It is always possible to retrieve statistics for a query with the *getExtra* method:

    @startDocuBlockInline 03_workWithAQL_getExtra
    @EXAMPLE_ARANGOSH_OUTPUT{03_workWithAQL_getExtra}
    |db._query(`FOR i IN 1..100
    |             INSERT { _key: CONCAT('test', TO_STRING(i)) }
    |                INTO mycollection`
      ).getExtra();
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 03_workWithAQL_getExtra

The meaning of the statistics values is described in [Execution statistics](../ExecutionAndPerformance/QueryStatistics.md).
You also will find warnings in here; If you're designing queries on the shell be sure to also look at it.

#### Setting a memory limit

To set a memory limit for the query, pass *options* to the *_query* method.
The memory limit specifies the maximum number of bytes that the query is
allowed to use. When a single AQL query reaches the specified limit value, 
the query will be aborted with a *resource limit exceeded* exception. In a 
cluster, the memory accounting is done per shard, so the limit value is 
effectively a memory limit per query per shard.

    @startDocuBlockInline 02_workWithAQL_memoryLimit
    @EXAMPLE_ARANGOSH_OUTPUT{02_workWithAQL_memoryLimit}
    |db._query(
    | 'FOR i IN 1..100000 SORT i RETURN i', {}, {
    |   memoryLimit: 100000
    }).toArray(); // xpError(ERROR_RESOURCE_LIMIT)
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 02_workWithAQL_memoryLimit

If no memory limit is specified, then the server default value (controlled by
startup option *--query.memory-limit* will be used for restricting the maximum amount 
of memory the query can use. A memory limit value of *0* means that the maximum
amount of memory for the query is not restricted. 

#### Setting options

There are further options that can be passed in the *options* attribute of the *_query* method:

- *failOnWarning*: when set to *true*, this will make the query throw an exception and
  abort in case a warning occurs. This option should be used in development to catch
  errors early. If set to *false*, warnings will not be propagated to exceptions and
  will be returned with the query results. There is also a server configuration option 
  `--query.fail-on-warning` for setting the default value for *failOnWarning* so it does
  not need to be set on a per-query level.

- *cache*: if set to *true*, this will put the query result into the query result cache
  if the query result is eligible for caching and the query cache is running in demand 
  mode. If set to *false*, the query result will not be inserted into the query result
  cache. Note that query results will never be inserted into the query result cache if
  the query result cache is disabled, and that they will be automatically inserted into
  the query result cache when it is active in non-demand mode.

- *profile*: if set to *true*, returns extra timing information for the query. The timing
  information is accessible via the *getExtra* method of the query result.

- *maxWarningCount*: limits the number of warnings that are returned by the query if
  *failOnWarning* is not set to *true*. The default value is *10*.

- *maxNumberOfPlans*: limits the number of query execution plans the optimizer will
  create at most. Reducing the number of query execution plans may speed up query plan
  creation and optimization for complex queries, but normally there is no need to adjust
  this value.

The following additional attributes can be passed to queries in the RocksDB storage engine:
 
- *maxTransactionSize*: transaction size limit in bytes

- *intermediateCommitSize*: maximum total size of operations after which an intermediate
  commit is performed automatically

- *intermediateCommitCount*: maximum number of operations after which an intermediate
  commit is performed automatically

In the ArangoDB Enterprise Edition there is an additional parameter:

- *skipInaccessibleCollections* AQL queries (especially graph traversals) will treat
  collection to which a user has **no access** rights as if these collections were empty.
  Instead of returning a *forbidden access* error, your queries will execute normally.
  This is intended to help with certain use-cases: A graph contains several collections
  and different users execute AQL queries on that graph. You can now naturally limit the 
  accessible results by changing the access rights of users on collections.

### with _createStatement (ArangoStatement)

The *_query* method is a shorthand for creating an ArangoStatement object,
executing it and iterating over the resulting cursor. If more control over the
result set iteration is needed, it is recommended to first create an
ArangoStatement object as follows:

    @startDocuBlockInline 04_workWithAQL_statements1
    @EXAMPLE_ARANGOSH_OUTPUT{04_workWithAQL_statements1}
    |stmt = db._createStatement( {
        "query": "FOR i IN [ 1, 2 ] RETURN i * 2" } );
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 04_workWithAQL_statements1

To execute the query, use the *execute* method of the statement:

    @startDocuBlockInline 05_workWithAQL_statements2
    @EXAMPLE_ARANGOSH_OUTPUT{05_workWithAQL_statements2}
    ~var stmt = db._createStatement( { "query": "FOR i IN [ 1, 2 ] RETURN i * 2" } );
    c = stmt.execute();
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 05_workWithAQL_statements2

#### Cursors

Once the query executed the query results are available in a cursor. 
The cursor can return all its results at once using the *toArray* method.
This is a short-cut that you can use if you want to access the full result
set without iterating over it yourself.

    @startDocuBlockInline 05_workWithAQL_statements3
    @EXAMPLE_ARANGOSH_OUTPUT{05_workWithAQL_statements3}
    ~var stmt = db._createStatement( { "query": "FOR i IN [ 1, 2 ] RETURN i * 2" } );
    ~var c = stmt.execute();
    c.toArray();
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 05_workWithAQL_statements3
    


Cursors can also be used to iterate over the result set document-by-document.
To do so, use the *hasNext* and *next* methods of the cursor:

    @startDocuBlockInline 05_workWithAQL_statements4
    @EXAMPLE_ARANGOSH_OUTPUT{05_workWithAQL_statements4}
    ~var stmt = db._createStatement( { "query": "FOR i IN [ 1, 2 ] RETURN i * 2" } );
    ~var c = stmt.execute();
    while (c.hasNext()) { require("@arangodb").print(c.next()); }
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 05_workWithAQL_statements4

Please note that you can iterate over the results of a cursor only once, and that
the cursor will be empty when you have fully iterated over it. To iterate over
the results again, the query needs to be re-executed.
 
Additionally, the iteration can be done in a forward-only fashion. There is no 
backwards iteration or random access to elements in a cursor.    

#### ArangoStatement parameters binding

To execute an AQL query using bind parameters, you need to create a statement first
and then bind the parameters to it before execution:

    @startDocuBlockInline 05_workWithAQL_statements5
    @EXAMPLE_ARANGOSH_OUTPUT{05_workWithAQL_statements5}
    |var stmt = db._createStatement( {
        "query": "FOR i IN [ @one, @two ] RETURN i * 2" } );
    stmt.bind("one", 1);
    stmt.bind("two", 2);
    c = stmt.execute();
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 05_workWithAQL_statements5

The cursor results can then be dumped or iterated over as usual, e.g.:
    
    @startDocuBlockInline 05_workWithAQL_statements6
    @EXAMPLE_ARANGOSH_OUTPUT{05_workWithAQL_statements6}
    ~var stmt = db._createStatement( { "query": "FOR i IN [ @one, @two ] RETURN i * 2" } );
    ~stmt.bind("one", 1);
    ~stmt.bind("two", 2);
    ~var c = stmt.execute();
    c.toArray();
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 05_workWithAQL_statements6

or 

    @startDocuBlockInline 05_workWithAQL_statements7
    @EXAMPLE_ARANGOSH_OUTPUT{05_workWithAQL_statements7}
    ~var stmt = db._createStatement( { "query": "FOR i IN [ @one, @two ] RETURN i * 2" } );
    ~stmt.bind("one", 1);
    ~stmt.bind("two", 2);
    ~var c = stmt.execute();
    while (c.hasNext()) { require("@arangodb").print(c.next()); }
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 05_workWithAQL_statements7

Please note that bind parameters can also be passed into the *_createStatement* method directly,
making it a bit more convenient:
    
    @startDocuBlockInline 05_workWithAQL_statements8
    @EXAMPLE_ARANGOSH_OUTPUT{05_workWithAQL_statements8}
    |stmt = db._createStatement( { 
    |  "query": "FOR i IN [ @one, @two ] RETURN i * 2", 
    |  "bindVars": { 
    |    "one": 1, 
    |    "two": 2 
    |  } 
    } );
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 05_workWithAQL_statements8

#### Counting with a cursor
    
Cursors also optionally provide the total number of results. By default, they do not. 
To make the server return the total number of results, you may set the *count* attribute to 
*true* when creating a statement:
    
    @startDocuBlockInline 05_workWithAQL_statements9
    @EXAMPLE_ARANGOSH_OUTPUT{05_workWithAQL_statements9}
    |stmt = db._createStatement( {
    | "query": "FOR i IN [ 1, 2, 3, 4 ] RETURN i",
      "count": true } );
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 05_workWithAQL_statements9

After executing this query, you can use the *count* method of the cursor to get the 
number of total results from the result set:

    @startDocuBlockInline 05_workWithAQL_statements10
    @EXAMPLE_ARANGOSH_OUTPUT{05_workWithAQL_statements10}
    ~var stmt = db._createStatement( { "query": "FOR i IN [ 1, 2, 3, 4 ] RETURN i", "count": true } );
    var c = stmt.execute();
    c.count();
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 05_workWithAQL_statements10

Please note that the *count* method returns nothing if you did not specify the *count*
attribute when creating the query.

This is intentional so that the server may apply optimizations when executing the query and 
construct the result set incrementally. Incremental creation of the result sets
is no possible
if all of the results need to be shipped to the client anyway. Therefore, the client
has the choice to specify *count* and retrieve the total number of results for a query (and
disable potential incremental result set creation on the server), or to not retrieve the total
number of results and allow the server to apply optimizations.

Please note that at the moment the server will always create the full result set for each query so 
specifying or omitting the *count* attribute currently does not have any impact on query execution.
This may change in the future. Future versions of ArangoDB may create result sets incrementally 
on the server-side and may be able to apply optimizations if a result set is not fully fetched by 
a client.


#### Using cursors to obtain additional information on internal timings

Cursors can also optionally provide statistics of the internal execution phases. By default, they do not. 
To get to know how long parsing, otpimisation, instanciation and execution took,
make the server return that by setting the *profile* attribute to
*true* when creating a statement:

    @startDocuBlockInline 06_workWithAQL_statements11
    @EXAMPLE_ARANGOSH_OUTPUT{06_workWithAQL_statements11}
    |stmt = db._createStatement( {
    | "query": "FOR i IN [ 1, 2, 3, 4 ] RETURN i",
      options: {"profile": true}} );
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 06_workWithAQL_statements11

After executing this query, you can use the *getExtra()* method of the cursor to get the 
produced statistics:

    @startDocuBlockInline 06_workWithAQL_statements12
    @EXAMPLE_ARANGOSH_OUTPUT{06_workWithAQL_statements12}
    ~var stmt = db._createStatement( { "query": "FOR i IN [ 1, 2, 3, 4 ] RETURN i", options: {"profile": true}} );
    var c = stmt.execute();
    c.getExtra();
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 06_workWithAQL_statements12

