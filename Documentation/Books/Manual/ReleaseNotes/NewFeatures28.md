Features and Improvements in ArangoDB 2.8
=========================================

The following list shows in detail which features have been added or improved in
ArangoDB 2.8. ArangoDB 2.8 also contains several bugfixes that are not listed
here. For a list of bugfixes, please consult the
[CHANGELOG](https://github.com/arangodb/arangodb/blob/devel/CHANGELOG).

AQL improvements
----------------

### AQL Graph Traversals / Pattern Matching

AQL offers a new feature to traverse over a graph without writing JavaScript functions
but with all the other features you know from AQL. For this purpose, a special version of 
`FOR variableName IN expression` has been introduced.

This special version has the following format: `FOR vertex-variable, edge-variable, path-variable IN traversal-expression`,
where `traversal-expression` has the following format:
`[depth] direction start-vertex graph-definition`
with the following input parameters:

* depth (optional): defines how many steps are executed.
  The value can either be an integer value (e.g. `3`) or a range of integer values (e.g. `1..5`). The default is 1. 
* direction: defines which edge directions are followed. Can be either `OUTBOUND`, `INBOUND` or `ANY`.
* start-vertex: defines where the traversal is started. Must be an `_id` value or a document.
* graph-definition: defines which edge collections are used for the traversal.
  Must be either `GRAPH graph-name` for graphs created with the graph-module, or a list of edge collections `edge-col1, edge-col2, .. edge-colN`.
  
The three output variables have the following semantics:

* vertex-variable: The last visited vertex.
* edge-variable: The last visited edge (optional).
* path-variable: The complete path from start-vertex to vertex-variable (optional).

The traversal statement can be used in the same way as the original `FOR variableName IN expression`,
and can be combined with filters and other AQL constructs.

As an example one can now find the friends of a friend for a certain user with this AQL statement:

```
FOR foaf, e, path IN 2 ANY @startUser GRAPH "relations"
  FILTER path.edges[0].type == "friend"
  FILTER path.edges[1].type == "friend"
  FILTER foaf._id != @startUser
  RETURN DISTINCT foaf
```

Optimizer rules have been implemented to gain performance of the traversal statement.
These rules move filter statements into the traversal statement s.t. paths which can never
pass the filter are not emitted to the variables.

As an example take the query above and assume there are edges that do not have `type == "friend"`.
If in the first edge step there is such a non-friend edge the second steps will never
be computed for these edges as they cannot fulfill the filter condition.

### Array Indexes

Hash indexes and skiplist indexes can now optionally be defined for array values 
so that they index individual array members instead of the entire array value.

To define an index for array values, the attribute name is extended with the
expansion operator `[*]` in the index definition.

Example:

```
db._create("posts");
db.posts.ensureHashIndex("tags[*]");
```

When given the following document

```json
{
  "tags": [
    "AQL",
    "ArangoDB",
    "Index"
  ]
}
```

this index will now contain the individual values `"AQL"`, `"ArangoDB"` and `"Index"`.

Now the index can be used for finding all documents having `"ArangoDB"` somewhere in 
their `tags` array using the following AQL query:

```
FOR doc IN posts
  FILTER "ArangoDB" IN doc.tags[*]
  RETURN doc
```

It is also possible to create an index on sub-attributes of array values. This makes 
sense when the index attribute is an array of objects, e.g.

```js
db._drop("posts");
db._create("posts");
db.posts.ensureIndex({ type: "hash", fields: [ "tags[*].name" ] });
db.posts.insert({ tags: [ { name: "AQL" }, { name: "ArangoDB" }, { name: "Index" } ] });
db.posts.insert({ tags: [ { name: "AQL" }, { name: "2.8" } ] });
```

The following query will then use the array index:

```
FOR doc IN posts
  FILTER 'AQL' IN doc.tags[*].name
  RETURN doc
```

Array values will automatically be de-duplicated before being inserted into an array index.

Please note that filtering using array indexes only works from within AQL queries and
only if the query filters on the indexed attribute using the `IN` operator. The other
comparison operators (`==`, `!=`, `>`, `>=`, `<`, `<=`) currently do not use array
indexes.


### Optimizer improvements

The AQL query optimizer can now use indexes if multiple filter conditions on attributes of
the same collection are combined with logical ORs, and if the usage of indexes would completely
cover these conditions.

For example, the following queries can now use two independent indexes on `value1` and `value2`
(the latter query requires that the indexes are skiplist indexes due to usage of the `<` and `>`
comparison operators):

```
FOR doc IN collection FILTER doc.value1 == 42 || doc.value2 == 23 RETURN doc
FOR doc IN collection FILTER doc.value1 < 42 || doc.value2 > 23 RETURN doc
```

The new optimizer rule "sort-in-values" can now pre-sort the right-hand side operand 
of `IN` and `NOT IN` operators so the operation can use a binary search with logarithmic 
complexity instead of a linear search. The rule will be applied when the right-hand side
operand of an `IN` or `NOT IN` operator in a filter condition is a variable that is defined 
in a different loop/scope than the operator itself. Additionally, the filter condition must 
consist of solely the `IN` or `NOT IN` operation in order to avoid any side-effects. 

The rule will kick in for a queries such as the following:

```
LET values = /* some runtime expression here */
FOR doc IN collection
  FILTER doc.value IN values
  RETURN doc
```

It will not be applied for the followig queries, because the right-hand side operand of the
`IN` is either not a variable, or because the FILTER condition may have side effects:

```
FOR doc IN collection
  FILTER doc.value IN /* some runtime expression here */
  RETURN doc
```

```
LET values = /* some runtime expression here */
FOR doc IN collection
  FILTER FUNCTION(doc.values) == 23 && doc.value IN values
  RETURN doc
```


### AQL functions added

The following AQL functions have been added in 2.8:

* `POW(base, exponent)`: returns the *base* to the exponent *exp*

* `UNSET_RECURSIVE(document, attributename, ...)`: recursively removes the attributes 
  *attributename* (can be one or many) from *document* and its sub-documents. All other 
  attributes will be preserved.
  Multiple attribute names can be specified by either passing multiple individual string argument 
  names, or by passing an array of attribute names:

      UNSET_RECURSIVE(doc, '_id', '_key', 'foo', 'bar')
      UNSET_RECURSIVE(doc, [ '_id', '_key', 'foo', 'bar' ])

* `IS_DATESTRING(value)`: returns true if *value* is a string that can be used in a date function. 
  This includes partial dates such as *2015* or *2015-10* and strings containing
  invalid dates such as *2015-02-31*. The function will return false for all
  non-string values, even if some of them may be usable in date functions.


### Miscellaneous improvements

* the ArangoShell now provides the convenience function `db._explain(query)` for retrieving 
  a human-readable explanation of AQL queries. This function is a shorthand for
  `require("org/arangodb/aql/explainer").explain(query)`.

* the AQL query optimizer now automatically converts `LENGTH(collection-name)` to an optimized 
  expression that returns the number of documents in a collection. Previous versions of 
  ArangoDB returned a warning when using this expression and also enumerated all documents
  in the collection, which was inefficient.

* improved performance of skipping over many documents in an AQL query when no
  indexes and no filters are used, e.g.

      FOR doc IN collection
        LIMIT 1000000, 10
        RETURN doc

* added cluster execution site info in execution plan explain output for AQL queries

* for 30+ AQL functions there is now an additional implementation in C++ that removes
  the need for internal data conversion when the function is called

* the AQL editor in the web interface now supports using bind parameters


Deadlock detection
------------------

ArangoDB 2.8 now has an automatic deadlock detection for transactions.

A deadlock is a situation in which two or more concurrent operations (user transactions
or AQL queries) try to access the same resources (collections, documents) and need to 
wait for the others to finish, but none of them can make any progress.

In case of such a deadlock, there would be no progress for any of the involved
transactions, and none of the involved transactions could ever complete. This is
completely undesirable, so the new automatic deadlock detection mechanism in ArangoDB
will automatically kick in and abort one of the transactions involved in such a deadlock. 
Aborting means that all changes done by the transaction will be rolled back and error 
29 (`deadlock detected`) will be thrown.

Client code (AQL queries, user transactions) that accesses more than one collection 
should be aware of the potential of deadlocks and should handle the error 29
(`deadlock detected`) properly, either by passing the exception to the caller or 
retrying the operation.


Replication
-----------

The following improvements for replication have been made in 2.8 (note: most of them
have been backported to ArangoDB 2.7 as well):

* added `autoResync` configuration parameter for continuous replication.

  When set to `true`, a replication slave will automatically trigger a full data 
  re-synchronization with the master when the master cannot provide the log data 
  the slave had asked for. Note that `autoResync` will only work when the option
  `requireFromPresent` is also set to `true` for the continuous replication, or
  when the continuous syncer is started and detects that no start tick is present.

  Automatic re-synchronization may transfer a lot of data from the master to the
  slave and may be expensive. It is therefore turned off by default.
  When turned off, the slave will never perform an automatic re-synchronization
  with the master.

* added `idleMinWaitTime` and `idleMaxWaitTime` configuration parameters for
  continuous replication.

  These parameters can be used to control the minimum and maximum wait time the
  slave will (intentionally) idle and not poll for master log changes in case the 
  master had sent the full logs already.
  The `idleMaxWaitTime` value will only be used when `adapativePolling` is set
  to `true`. When `adaptivePolling` is disabled, only `idleMinWaitTime` will be
  used as a constant time span in which the slave will not poll the master for
  further changes. The default values are 0.5 seconds for `idleMinWaitTime` and
  2.5 seconds for `idleMaxWaitTime`, which correspond to the hard-coded values
  used in previous versions of ArangoDB.

* added `initialSyncMaxWaitTime` configuration parameter for initial and continuous
  replication

  This option controls the maximum wait time (in seconds) that the initial
  synchronization will wait for a response from the master when fetching initial 
  collection data. If no response is received within this time period, the initial
  synchronization will give up and fail. This option is also relevant for
  continuous replication in case *autoResync* is set to *true*, as then the
  continuous replication may trigger a full data re-synchronization in case
  the master cannot the log data the slave had asked for.

* HTTP requests sent from the slave to the master during initial synchronization
  will now be retried if they fail with connection problems.

* the initial synchronization now logs its progress so it can be queried using
  the regular replication status check APIs.

* added `async` attribute for `sync` and `syncCollection` operations called from
  the ArangoShell. Setthing this attribute to `true` will make the synchronization 
  job on the server go into the background, so that the shell does not block. The
  status of the started asynchronous synchronization job can be queried from the 
  ArangoShell like this:

      /* starts initial synchronization */
      var replication = require("org/arangodb/replication");
      var id = replication.sync({
        endpoint: "tcp://master.domain.org:8529",
        username: "myuser",
        password: "mypasswd",
        async: true
      });

      /* now query the id of the returned async job and print the status */
      print(replication.getSyncResult(id));

  The result of `getSyncResult()` will be `false` while the server-side job
  has not completed, and different to `false` if it has completed. When it has
  completed, all job result details will be returned by the call to `getSyncResult()`.

* the web admin interface dashboard now shows a server's replication status
  at the bottom of the page


Web Admin Interface
-------------------

The following improvements have been made for the web admin interface:

* the AQL editor now has support for bind parameters. The bind parameter values can
  be edited in the web interface and saved with a query for future use.

* the AQL editor now allows canceling running queries. This can be used to cancel
  long-running queries without switching to the *query management* section.

* the dashboard now provides information about the server's replication status at
  the bottom of the page. This can be used to track either the status of a one-time
  synchronization or the continuous replication.

* the compaction status and some status internals about collections are now displayed
  in the detail view for a collection in the web interface. These data can be used
  for debugging compaction issues.

* unloading a collection via the web interface will now trigger garbage collection
  in all v8 contexts and force a WAL flush. This increases the chances of perfoming
  the unload faster.

* the status terminology for collections for which an unload request has been issued 
  via the web interface was changed from `in the process of being unloaded` to 
  `will be unloaded`. This is more accurate as the actual unload may be postponed
  until later if there are still references pointing to data in the collection.


Foxx improvements
-----------------

* the module resolution used by `require` now behaves more like in node.js

* the `org/arangodb/request` module now returns response bodies for error responses
  by default. The old behavior of not returning bodies for error responses can be
  re-enabled by explicitly setting the option `returnBodyOnError` to `false`


Miscellaneous changes
---------------------

The startup option `--server.hide-product-header` can be used to make the server 
not send the HTTP response header `"Server: ArangoDB"` in its HTTP responses. This
can be used to conceal the server make from HTTP clients.
By default, the option is turned off so the header is still sent as usual.

arangodump and arangorestore now have better error reporting. Additionally, arangodump 
will now fail by default when trying to dump edges that refer to already dropped 
collections. This can be circumvented by specifying the option `--force true` when 
invoking arangodump.

arangoimp now provides an option `--create-collection-type` to specify the type of 
the collection to be created when `--create-collection` is set to `true`. Previously
`--create-collection` always created document collections and the creation of edge
collections was not possible. 

