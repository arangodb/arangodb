---
layout: default
description: This chapter describes AQL features related to query executions and query performance
---
AQL Execution and Performance
=============================

This chapter describes AQL features related to query executions and query performance.

* [Execution statistics](execution-and-performance-query-statistics.html): A query that has been executed also returns statistics about its execution. 

* [Query parsing](execution-and-performance-parsing-queries.html): Clients can use ArangoDB to check if a given AQL query is syntactically valid. 

* [Query execution plan](execution-and-performance-explaining-queries.html): If it is unclear how a given query will perform, clients can retrieve a query's execution plan from the AQL query optimizer without actually executing the query; this is called explaining.

* [The AQL query optimizer](execution-and-performance-optimizer.html): AQL queries are sent through an optimizer before execution. The task of the optimizer is to create an initial execution plan for the query, look for optimization opportunities and apply them.

* [Query Profiling](execution-and-performance-query-profiler.html): Sometimes a query does not perform, but it is unclear which 
parts of the plan are responsible. The query-profiler can show you execution statistics for every
stage of the query execution.

* [The AQL query result cache](execution-and-performance-query-cache.html): an optional query results cache can be used to avoid repeated calculation of the same query results.

Be sure to check out the
[ArangoDB Performance Course](https://www.arangodb.com/arangodb-performance-course/){:target="_blank"}
for freshers as well.
