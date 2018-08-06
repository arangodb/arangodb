# Profiling and Hand-Optimizing AQL queries

To give you more insight into your query we now allow you to execute your query with special instrumentation code enabled.
This will then print a query plan with detailed execution statistics. To use this in an interactive fashion on the shell you can use the `db._profileQuery(..)` in the arangosh. Alternatively there is a new button _Profile_ in Query tab of the WebUI.

The printed execution plan then contains three additional columns:

- **Call**: The number of times this query stage was executed
- **Items**: The number of temporary result rows at this stage
- **Runtime**: The total time spend in this stage 

Below the execution plan there are additional sections for the overall runtime statistics and the query profile.

## Example: Simple AQL query

Assuming we got a collection named `acollection` and insert 10000 documents
via `for (let i=0; i < 10000;i++) db.acollection.insert({value:i})`. Then a simple
query filtering for `value < 10` will return 10 results:

@startDocuBlockInline 01_workWithAQL_profileQuerySimple
@EXAMPLE_ARANGOSH_OUTPUT{01_workWithAQL_profileQuerySimple}
~db._drop("acollection");
~db._create('acollection');
~for (let i=0; i < 10000;i++) { db.acollection.insert({value:i}); }
|db._profileQuery(`
|FOR doc IN acollection
|  FILTER doc.value < 10
|  RETURN doc`, {}, {colors: false});
~db._drop("acollection");
@END_EXAMPLE_ARANGOSH_OUTPUT
@endDocuBlock 01_workWithAQL_profileQuerySimple

An AQL query is essentially executed in a pipeline that chains togehter different
functional execution blocks. Each block gets the input rows from the parent above it, 
does some processing and then outputs a certain number of output rows.

Without any detailed insight into the query execution it is impossible to tell how many results each pipeline-block
had to work on and how long this took. By executing the query with the query profiler (`db._profileQuery` or via
the _Profile_ button in the WebUI) you can now tell exactly how much work each stage had to do.

Without any indexes this query should have to perform the following operations:

1. Perfom a full collection scan via a _EnumerateCollectionNode_ and outputing a row containg the document in `doc`.
2. Calculate the boolean expression `LET #1 = doc.value < 10 `from all inputs via a _CalculationNode_ 
3. Filter out all input rows where `#1` is false via the _FilterNode_
4. Put the `doc` variable of the remaining rows into the result set via the _ResultNode_

The _EnumerateCollectionNode_ processed and returned all 10k rows (documents), as did the _CalculationNode_.
Because the AQL execution engine also uses an internal batch size of 1000 these blocks were also called 100 times each.
The _FilterNode_ as well as the _ReturnNode_ however only ever returned 10 rows and only had to be called once, because
the result size fit within a single batch.

Now lets add a skiplist index on `value` to speedup the query.
```JavaScript
db.acollection.ensureIndex({type:"skiplist", fields:["value"]});
```

@startDocuBlockInline 02_workWithAQL_profileQuerySimpleIndex
@EXAMPLE_ARANGOSH_OUTPUT{02_workWithAQL_profileQuerySimpleIndex}
~db._create('acollection');
~db.acollection.ensureIndex({type:"skiplist", fields:["value"]});
~for (let i=0; i < 10000;i++) { db.acollection.insert({value:i}); }
|db._profileQuery(`
|FOR doc IN acollection
|  FILTER doc.value < 10
|  RETURN doc`, {}, {colors: false});
~db._drop("acollection");
@END_EXAMPLE_ARANGOSH_OUTPUT
@endDocuBlock 02_workWithAQL_profileQuerySimpleIndex

The results in replacing the collection scan and filter block with an `IndexNode`. The execution pipeleine of the AQL
query has become much shorter. Also the number of rows processed by each pipeline block is only 10, because we no longer need to look at all documents.


## Example: AQL with Subquery

Lets consider a query containing a SubQuery 

@startDocuBlockInline 03_workWithAQL_profileQuerySubquery
@EXAMPLE_ARANGOSH_OUTPUT{03_workWithAQL_profileQuerySubquery}
~db._create('acollection');
~db.acollection.ensureIndex({type:"skiplist", fields:["value"]});
~for (let i=0; i < 10000;i++) { db.acollection.insert({value:i}); }
|db._profileQuery(`
|LET list = (FOR doc in acollection FILTER doc.value > 90 RETURN doc)
|FOR a IN list 
|   FILTER a.value < 91 
|   RETURN a`, {}, {colors: false, optimizer:{rules:["-all"]}});;
~db._drop("acollection");
@END_EXAMPLE_ARANGOSH_OUTPUT
@endDocuBlock 03_workWithAQL_profileQuerySubquery

The resulting query profile contains a _SubqueryNode_ which has the runtime of all its children combined.
Actually I cheated a little, the optimizer would have completely removed the subquery, if I had not deactivated it.
The optimimized version would take longer in the "optimizing plan" stage, but should perform better with a lot of results.

## Example: AQL with Aggregation

Lets try a more advanced example, using a [COLLECT](https://docs.arangodb.com/devel/AQL/Operations/Collect.html) statement.
Assuming we have a user collection with a city, username and an age value.
Now lets try a query which gets us all age groups in buckets (0-9, 10-19, 20-29, ...):

@startDocuBlockInline 04_workWithAQL_profileQueryAggregation
@EXAMPLE_ARANGOSH_OUTPUT{04_workWithAQL_profileQueryAggregation}
~db._create('myusers');
|["berlin", "paris", "cologne", "munich", "london"].forEach((c) => {
|  ["peter", "david", "simon", "lars"].forEach( 
|    n => db.myusers.insert({ city : c, name : n, age: Math.floor(Math.random() * 75) })
|  )
|});
|db._profileQuery(`
|FOR u IN myusers
|  COLLECT ageGroup = FLOOR(u.age / 10) * 10
|  AGGREGATE minAge = MIN(u.age), maxAge = MAX(u.age), len = LENGTH(u)
|  RETURN {
|    ageGroup, 
|    minAge, 
|    maxAge,
|    len
|  }`, {}, {colors: false});
~db._drop("myusers")
@END_EXAMPLE_ARANGOSH_OUTPUT
@endDocuBlock 04_workWithAQL_profileQueryAggregation

Without any indexes this query should have to perform the following operations:

1. Perfom a full collection scan via a _EnumerateCollectionNode_ and outputing a row containg the document in `doc`.
2. Calculate expressions `LET #1 = FLOOR(u.age / 10) * 10 `, etc from all inputs via multiple _CalculationNode_ 
3. Perform the aggregations via the _CollectNode_
4. Sort the resulting aggregated rows via a _SortNode_
5. Build a result value  via another _CalculationNode_
6. Put the result variable into the result set via the _ResultNode_

Similary to above you can see that after the _CalculationNode_ stage, from the original 20 rows only
a handful remained.

## Typical AQL Performance Mistakes

With the new query profilier you should be able to spot typical performance mistakes
that we see often with our customers:

- Not employing indexes to speed up queries with common filter expressions
- Not using shard keys in filter statements, when it is known (Only a cluster problem)
- Using subqueries to calculate an intermediary result, but only using a few results

Bad example:
```
LET vertices = (FOR v IN 1..2 ANY @startVertex GRAPH 'my_graph' RETURN v)  // <-- add a LIMIT 1 here
FOR doc IN collection
  FILTER doc.value == vertices[0].value
  RETURN doc
```
Adding a `LIMIT 1` into the subquery whould have probably gained some performance

- Starting a Graph Traversal from the wrong side (if both ends are known)

Assuming you have two vertex collections _users_, _products_ as well as an edge collection _purchased_.
The graph model  would look like this ` (users) <--[purchased]--> (products)`, i.e. every user is 
connected with an edge in _pruchased_ to zero or more _products_.

Assuming we want to know all users that have purchased the product _playstation_ as well as produts of 
`type` _legwarmer_ we could use this query:
```
FOR prod IN products
  FILTER prod.type == 'legwarmer'
  FOR v,e,p IN 2..2 OUTGOING prod purchased
    FILTER v._key == 'playstation' // <-- last vertex of the path
    RETURN p.vertices[1] // <-- the user
```

This query first finds all legwarmer products and then performs a traversal for each of them. But we could also
inverse the traversal by starting of with the known _playstation_ product. This way we only need a single traversal,
to achieve the same result

```
FOR v,e,p IN 2..2 OUTGOING 'product/playstation' purchased
  FILTER v.type == 'legwarmer' // <-- last vertex of the path
  RETURN p.vertices[1] // <-- the user
```