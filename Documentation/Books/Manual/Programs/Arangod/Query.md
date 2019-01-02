# ArangoDB Server Query Options

## Limiting memory for AQL queries

`--query.memory-limit value`

The default maximum amount of memory (in bytes) that a single AQL query can use.
When a single AQL query reaches the specified limit value, the query will be
aborted with a *resource limit exceeded* exception. In a cluster, the memory
accounting is done per shard, so the limit value is effectively a memory limit per
query per shard.

The global limit value can be overridden per query by setting the *memoryLimit*
option value for individual queries when running an AQL query.

The default value is *0*, meaning that there is no memory limit.

## Turning AQL warnings into errors

`--query.fail-on-warning value`

When set to *true*, AQL queries that produce warnings will instantly abort and
throw an exception. This option can be set to catch obvious issues with AQL
queries early. When set to *false*, AQL queries that produce warnings will not
abort and return the warnings along with the query results.
The option can also be overridden for each individual AQL query.

## Enable/disable AQL query tracking

`--query.tracking flag`

If *true*, the server's AQL slow query tracking feature will be enabled by
default. Tracking of queries can be disabled by setting the option to *false*.

The default is *true*.

## Enable/disable tracking of bind variables in AQL queries

`--query.tracking-with-bindvars flag`

If *true*, then the bind variables will be tracked and shown for all running 
and slow AQL queries. When set to *true*, this will also enable the display of
bind variable values in the list of cached AQL query results.
This option only has an effect if `--query.tracking` was set to *true* or when
the query results cache is used. 
Tracking and displaying bind variable values can be disabled by setting the option to *false*.

The default is *true*.

## Threshold for slow AQL queries

`--query.slow-threshold value`

By setting *value* it can be controlled after what execution time an AQL query
is considered "slow". Any slow queries that exceed the execution time specified
in *value* will be logged when they are finished. The threshold value is
specified in seconds. Tracking of slow queries can be turned off entirely by
setting the option `--query.tracking` to *false*.

The default value is *10.0*.

`--query.slow-streaming-threshold value`

By setting *value* it can be controlled after what execution time streaming AQL 
queries are considered "slow". This option exists to give streaming queries a
separate, potentially higher timeout value than regular queries. Streaming queries
are often executed in lockstep with application data processing logic, which then
also accounts for the queries' runtime. It is thus not unexpected if streaming 
queries' lifetime is longer than the one of regular queries.

The default value is *10.0*.

## Limiting the number of query execution plans created by the AQL optimizer

`--query.optimizer-max-plans value`

By setting *value* it can be controlled how many different query execution plans
the AQL query optimizer will generate at most for any given AQL query. Normally
the AQL query optimizer will generate a single execution plan per AQL query, but
there are some cases in which it creates multiple competing plans. More plans
can lead to better optimized queries, however, plan creation has its costs. The
more plans are created and shipped through the optimization pipeline, the more
time will be spent in the optimizer.
Lowering *value* will make the optimizer stop creating additional plans when it
has already created enough plans.
Note that this setting controls the default maximum number of plans to create. The
value can still be adjusted on a per-query basis by setting the *maxNumberOfPlans*
attribute when running a query.

The default value is *128*.

## AQL Query results caching mode

`--query.cache-mode`

Toggles the AQL query results cache behavior. Possible values are:

* *off*: do not use query results cache
* *on*: always use query results cache, except for queries that have their *cache*
  attribute set to *false*
* *demand*: use query results cache only for queries that have their *cache*
  attribute set to *true*

## AQL Query results cache size

`--query.cache-entries`

Maximum number of query results that can be stored per database-specific query
results cache. If a query is eligible for caching and the number of items in the
database's query cache is equal to this threshold value, another cached query
result will be removed from the cache.

This option only has an effect if the query cache mode is set to either *on* or
*demand*.

The default value is *128*.

`--query.cache-entries-max-size`

Maximum cumulated size of query results that can be stored per database-specific 
query results cache. When inserting a query result into the query results cache,
it is check if the total size of cached results would exceed this value, and if so,
another cached query result will be removed from the cache before inserting a new
one.

This option only has an effect if the query cache mode is set to either *on* or
*demand*.

The default value is *256 MB*.

`--query.cache-entry-max-size`

Maximum size of individual query results that can be stored in any database's query 
results cache. Query results are only eligible for caching when their size does not exceed
this setting's value.

The default value is *16 MB*.

`--query.cache-include-system-collections`

Whether or not to store results of queries that involve system collections in
the query results cache. Not storing these results is normally beneficial when using the
query results cache, as queries on system collections are internal to ArangoDB and will
only use space in the query results cache unnecessarily.

The default value is *false*.
