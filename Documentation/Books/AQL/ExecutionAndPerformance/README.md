AQL Execution and Performance
=============================

This chapter describes AQL features related to query executions and query performance.

* [Execution statistics](QueryStatistics.md): A query that has been executed also returns statistics about its execution. 

* [Query parsing](ParsingQueries.md): Clients can use ArangoDB to check if a given AQL query is syntactically valid. 

* [Query execution plan](ExplainingQueries.md): If it is unclear how a given query will perform, clients can retrieve a query's execution plan from the AQL query optimizer without actually executing the query; this is called explaining.

* [The AQL query optimizer](Optimizer.md): AQL queries are sent through an optimizer before execution. The task of the optimizer is to create an initial execution plan for the query, look for optimization opportunities and apply them.

* [Query Profiling](QueryProfiler.md): Sometimes a query does not perform, but it is unclear which 
parts of the plan are responsible. The query-profiler can show you execution statistics for every
stage of the query execution.

* [The AQL query result cache](QueryCache.md): an optional query results cache can be used to avoid repeated calculation of the same query results.

Be sure to check out the
[ArangoDB Performance Course](https://www.arangodb.com/arangodb-performance-course/)
for freshers as well.
