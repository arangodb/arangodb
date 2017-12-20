Query statistics
================

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



