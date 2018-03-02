How to invoke AQL
=================

Executing queries
-----------------

You can run AQL queries from your application via the HTTP REST API. The full
API description is available at [HTTP Interface for AQL Query Cursors](../HttpAqlQueryCursor/README.md).

You can also run AQL queries from arangosh. To do so, you can use the *_query* method 
of the *db* object. This will run the specified query in the context of the currently
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

It is also possible to use ES6 template strings for generating AQL queries. There is
a template string generator function named *aqlQuery*; we call it once to demonstrate
its result, and once putting it directly into the query:

```js
var key = 'testKey';
aqlQuery`FOR c IN mycollection FILTER c._key == ${key} RETURN c._key`;
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
      | aqlQuery`FOR c IN mycollection FILTER c._key == ${key} RETURN c._key`
        ).toArray();
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 02_workWithAQL_aqlQuery

Arbitrary JavaScript expressions can be used in queries that are generated with the 
*aqlQuery* template string generator. Collection objects are handled automatically:

    @startDocuBlockInline 02_workWithAQL_aqlCollectionQuery
    @EXAMPLE_ARANGOSH_OUTPUT{02_workWithAQL_aqlCollectionQuery}
      var key = 'testKey';
      |db._query(aqlQuery`FOR doc IN ${ db.mycollection } RETURN doc`
          ).toArray();
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 02_workWithAQL_aqlCollectionQuery

Note: data-modification AQL queries normally do not return a result (unless the AQL query 
contains an extra *RETURN* statement). When not using a *RETURN* statement in the query, the 
*toArray* method will return an empty array.

It is always possible to retrieve statistics for a query with the *getExtra* method:

    @startDocuBlockInline 03_workWithAQL_getExtra
    @EXAMPLE_ARANGOSH_OUTPUT{03_workWithAQL_getExtra}
    |db._query(`FOR i IN 1..100
    |             INSERT { _key: CONCAT('test', TO_STRING(i)) }
    |                INTO mycollection`
      ).getExtra();
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 03_workWithAQL_getExtra

The meaning of the statistics values is described below.

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

This has executed the query. The query results are available in a cursor
now. The cursor can return all its results at once using the *toArray* method.
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
    while (c.hasNext()) { require("internal").print(c.next()); }
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 05_workWithAQL_statements4

Please note that you can iterate over the results of a cursor only once, and that
the cursor will be empty when you have fully iterated over it. To iterate over
the results again, the query needs to be re-executed.

Additionally, the iteration can be done in a forward-only fashion. There is no
backwards iteration or random access to elements in a cursor.

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
    while (c.hasNext()) { require("internal").print(c.next()); }
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


Query statistics
----------------

A query that has been executed will always return execution statistics. Execution statistics
can be retrieved by calling `getExtra()` on the cursor. The statistics are returned in the
return value's `stats` attribute:

    @startDocuBlockInline 06_workWithAQL_statementsExtra
    @EXAMPLE_ARANGOSH_OUTPUT{06_workWithAQL_statementsExtra}
    |db._query(`
    |   FOR i IN 1..@count INSERT
    |     { _key: CONCAT('anothertest', TO_STRING(i)) }
    |     INTO mycollection`,
    |  {count: 100},
    |  {},
    |  {fullCount: true}
      ).getExtra();
    |db._query({
    |  "query": `FOR i IN 200..@count INSERT
    |              { _key: CONCAT('anothertest', TO_STRING(i)) }
    |              INTO mycollection`,
    |  "bindVars": {count: 300},
    |  "options": { fullCount: true}
      }).getExtra();
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 06_workWithAQL_statementsExtra

The meaning of the statistics attributes is as follows:

* *writesExecuted*: the total number of data-modification operations successfully executed.
  This is equivalent to the number of documents created, updated or removed by `INSERT`,
  `UPDATE`, `REPLACE` or `REMOVE` operations.
* *writesIgnored*: the total number of data-modification operations that were unsuccessful,
  but have been ignored because of query option `ignoreErrors`.
* *scannedFull*: the total number of documents iterated over when scanning a collection 
  without an index. Documents scanned by subqueries will be included in the result, but not
  no operations triggered by built-in or user-defined AQL functions.
* *scannedIndex*: the total number of documents iterated over when scanning a collection using
  an index. Documents scanned by subqueries will be included in the result, but not
  no operations triggered by built-in or user-defined AQL functions.
* *filtered*: the total number of documents that were removed after executing a filter condition
  in a `FilterNode`. Note that `IndexRangeNode`s can also filter documents by selecting only
  the required index range from a collection, and the `filtered` value only indicates how much
  filtering was done by `FilterNode`s.
* *fullCount*: the total number of documents that matched the search condition if the query's
  final `LIMIT` statement were not present.
  This attribute will only be returned if the `fullCount` option was set when starting the 
  query and will only contain a sensible value if the query contained a `LIMIT` operation on
  the top level.


Explaining queries
------------------

If it is unclear how a given query will perform, clients can retrieve a query's execution plan 
from the AQL query optimizer without actually executing the query. Getting the query execution 
plan from the optimizer is called *explaining*.

An explain will throw an error if the given query is syntactically invalid. Otherwise, it will
return the execution plan and some information about what optimizations could be applied to
the query. The query will not be executed.

Explaining a query can be achieved by calling the [HTTP REST API](../HttpAqlQuery/README.md).
A query can also be explained from the ArangoShell using `ArangoStatement`s `explain` method.

By default, the query optimizer will return what it considers to be the *optimal plan*. The
optimal plan will be returned in the `plan` attribute of the result. If `explain` is
called with option `allPlans` set to `true`, all plans will be returned in the `plans`
attribute instead. The result object will also contain an attribute *warnings*, which 
is an array of warnings that occurred during optimization or execution plan creation.

Each plan in the result is an object with the following attributes:
- *nodes*: the array of execution nodes of the plan. The list of available node types
   can be found [here](../Aql/Optimizer.md)
- *estimatedCost*: the total estimated cost for the plan. If there are multiple
  plans, the optimizer will choose the plan with the lowest total cost.
- *collections*: an array of collections used in the query
- *rules*: an array of rules the optimizer applied. The list of rules can be 
  found [here](../Aql/Optimizer.md)
- *variables*: array of variables used in the query (note: this may contain
  internal variables created by the optimizer)

Here is an example for retrieving the execution plan of a simple query:

    @startDocuBlockInline 07_workWithAQL_statementsExplain
    @EXAMPLE_ARANGOSH_OUTPUT{07_workWithAQL_statementsExplain}
    |var stmt = db._createStatement(
     "FOR user IN _users RETURN user");
    stmt.explain();
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 07_workWithAQL_statementsExplain

As the output of `explain` is very detailed, it is recommended to use some
scripting to make the output less verbose:

    @startDocuBlockInline 08_workWithAQL_statementsPlans
    @EXAMPLE_ARANGOSH_OUTPUT{08_workWithAQL_statementsPlans}
    |var formatPlan = function (plan) {
    |    return { estimatedCost: plan.estimatedCost,
    |        nodes: plan.nodes.map(function(node) {
                return node.type; }) }; };
    formatPlan(stmt.explain().plan);
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 08_workWithAQL_statementsPlans

If a query contains bind parameters, they must be added to the statement **before**
`explain` is called:

    @startDocuBlockInline 09_workWithAQL_statementsPlansBind
    @EXAMPLE_ARANGOSH_OUTPUT{09_workWithAQL_statementsPlansBind}
    |var stmt = db._createStatement(
    | `FOR doc IN @@collection FILTER doc.user == @user RETURN doc`
    );
    stmt.bind({ "@collection" : "_users", "user" : "root" });
    stmt.explain();
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 09_workWithAQL_statementsPlansBind

In some cases the AQL optimizer creates multiple plans for a single query. By default
only the plan with the lowest total estimated cost is kept, and the other plans are
discarded. To retrieve all plans the optimizer has generated, `explain` can be called
with the option `allPlans` set to `true`.

In the following example, the optimizer has created two plans:

    @startDocuBlockInline 10_workWithAQL_statementsPlansOptimizer0
    @EXAMPLE_ARANGOSH_OUTPUT{10_workWithAQL_statementsPlansOptimizer0}
    |var stmt = db._createStatement(
      "FOR user IN _users FILTER user.user == 'root' RETURN user");
    stmt.explain({ allPlans: true }).plans.length;
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 10_workWithAQL_statementsPlansOptimizer0

To see a slightly more compact version of the plan, the following transformation can be applied:

    @startDocuBlockInline 10_workWithAQL_statementsPlansOptimizer1
    @EXAMPLE_ARANGOSH_OUTPUT{10_workWithAQL_statementsPlansOptimizer1}
    ~var stmt = db._createStatement("FOR user IN _users FILTER user.user == 'root' RETURN user");
    |stmt.explain({ allPlans: true }).plans.map(
        function(plan) { return formatPlan(plan); });
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 10_workWithAQL_statementsPlansOptimizer1

`explain` will also accept the following additional options:
- *maxPlans*: limits the maximum number of plans that are created by the AQL query optimizer
- *optimizer.rules*: an array of to-be-included or to-be-excluded optimizer rules
  can be put into this attribute, telling the optimizer to include or exclude
  specific rules. To disable a rule, prefix its name with a `-`, to enable a rule, prefix it
  with a `+`. There is also a pseudo-rule `all`, which will match all optimizer rules.

The following example disables all optimizer rules but `remove-redundant-calculations`:

    @startDocuBlockInline 10_workWithAQL_statementsPlansOptimizer2
    @EXAMPLE_ARANGOSH_OUTPUT{10_workWithAQL_statementsPlansOptimizer2}
    ~var stmt = db._createStatement("FOR user IN _users FILTER user.user == 'root' RETURN user");
    |stmt.explain({ optimizer: {
       rules: [ "-all", "+remove-redundant-calculations" ] } });
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 10_workWithAQL_statementsPlansOptimizer2


The contents of an execution plan are meant to be machine-readable. To get a human-readable
version of a query's execution plan, the following commands can be used:

    @startDocuBlockInline 10_workWithAQL_statementsPlansOptimizer3
    @EXAMPLE_ARANGOSH_OUTPUT{10_workWithAQL_statementsPlansOptimizer3}
    var query = "FOR doc IN mycollection FILTER doc.value > 42 RETURN doc";
    require("org/arangodb/aql/explainer").explain(query, {colors:false});
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 10_workWithAQL_statementsPlansOptimizer3

The above command prints the query's execution plan in the ArangoShell directly, focusing
on the most important information.


Parsing queries
---------------

Clients can use ArangoDB to check if a given AQL query is syntactically valid. ArangoDB provides
an [HTTP REST API](../HttpAqlQuery/README.md) for this.

A query can also be parsed from the ArangoShell using `ArangoStatement`s `parse` method. The
`parse` method will throw an exception if the query is syntactically invalid. Otherwise, it will
return the some information about the query.

The return value is an object with the collection names used in the query listed in the
`collections` attribute, and all bind parameters listed in the `bindVars` attribute.
Additionally, the internal representation of the query, the query's abstract syntax tree, will
be returned in the `AST` attribute of the result. Please note that the abstract syntax tree
will be returned without any optimizations applied to it.

    @startDocuBlockInline 11_workWithAQL_parseQueries
    @EXAMPLE_ARANGOSH_OUTPUT{11_workWithAQL_parseQueries}
    |var stmt = db._createStatement(
      "FOR doc IN @@collection FILTER doc.foo == @bar RETURN doc");
    stmt.parse();
    ~removeIgnoreCollection("mycollection")
    ~db._drop("mycollection")
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 11_workWithAQL_parseQueries
