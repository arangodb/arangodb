!CHAPTER The AQL query result cache

AQL provides an optional query result cache.

The purpose of the query cache is to avoid repeated calculation of the same
query results. It is useful if data-reading queries repeat a lot and there are
not many write queries.

The query cache is transparent so users do not need to manually invalidate 
results in it if underlying collection data are modified. 


!SECTION Modes

The cache can be operated in the following modes:

* `off`: the cache is disabled. No query results will be stored
* `on`: the cache will store the results of all AQL queries unless their `cache`
  attribute flag is set to `false`
* `demand`: the cache will store the results of AQL queries that have their
  `cache` attribute set to `true`, but will ignore all others

The mode can be set at server startup and later changed at runtime.


!SECTION Query eligibility

The query cache will consider two queries identical if they have exactly the
same query string. Any deviation in terms of whitespace, capitalization etc.
will be considered a difference. The query string will be hashed and used as
the cache lookup key. If a query uses bind parameters, these will also be hashed
and used as the cache lookup key.

That means even if the query string for two queries is identical, the query
cache will treat them as different queries if they have different bind parameter
values. Other components that will become part of a query's cache key are the
`count` and `fullCount` attributes.

If the cache is turned on, the cache will check at the very start of execution
whether it has a result ready for this particular query. If that is the case,
the query result will be served directly from the cache, which is normally
very efficient. If the query cannot be found in the cache, it will be executed
as usual.

If the query is eligible for caching and the cache is turned on, the query
result will be stored in the query cache so it can be used for subsequent 
executions of the same query.

A query is eligible for caching only if all of the following conditions are met:

* the server the query executes on is not a coordinator
* the query string is at least 8 characters long 
* the query is a read-only query and does not modify data in any collection
* no warnings were produced while executing the query
* the query is deterministic and only uses deterministic functions

The usage of non-deterministic functions leads to a query not being cachable. 
This is intentional to avoid caching of function results which should rather
be calculated on each invocation of the query (e.g. `RAND()` or `DATE_NOW()`).

The query cache considers all user-defined AQL functions to be non-deterministic
as it has no insight into these functions.


!SECTION Cache invalidation

The query cache results are fully or partially invalidated automatically if
queries modify the data of collections that were used during the computation of
the cached query results. This is to protect users from getting stale results
from the query cache.

This also means that if the cache is turned on, then there is an additional
cache invalidation check for each data-modification operation (e.g. insert, update, 
remove, truncate operations as well as AQL data-modification queries).

**Example**

If the result of the following query is present in the query cache,
then either modifying data in collection `users` or in collection `organizations`
will remove the already computed result from the cache:

```
FOR user IN users
  FOR organization IN organizations
    FILTER user.organization == organization._key
    RETURN { user: user, organization: organization }
```

Modifying data in other collections than the named two will not lead to this
query result being removed from the cache.


!SECTION Performance considerations

The query cache is organized as a hash table, so looking up whether a query result
is present in the cache is relatively fast. Still, the query string and the bind
parameter used in the query will need to be hashed. This is a slight overhead that
will not be present if the cache is turned off or a query is marked as not cacheable.

Additionally, storing query results in the cache and fetching results from the 
cache requires locking via an R/W lock. While many thread can read in parallel from
the cache, there can only be a single modifying thread at any given time. Modifications
of the query cache contents are required when a query result is stored in the cache
or during cache invalidation after data-modification operations. Cache invalidation
will require time proportional to the number of cached items that need to be invalidated.

There may be workloads in which enabling the query cache will lead to a performance
degradation. It is not recommended to turn the query cache on in workloads that only
modify data, or that modify data more often than reading it. Turning on the query cache
will also provide no benefit if queries are very diverse and do not repeat often.
In read-only or read-mostly workloads, the query cache will be beneficial if the same
queries are repeated lots of times.

In general, the query cache will provide the biggest improvements for queries with
small result sets that take long to calculate. If query results are very big and
most of the query time is spent on copying the result from the cache to the client,
then the cache will not provide much benefit.


!SECTION Global configuration

The query cache can be configured at server start using the configuration parameter
`--query.cache-mode`. This will set the cache mode according to the descriptions
above. 

After the server is started, the cache mode can be changed at runtime as follows:

```
require("@arangodb/aql/cache").properties({ mode: "on" }); 
```

The maximum number of cached results in the cache for each database can be configured
at server start using the configuration parameter `--query.cache-entries`.
This parameter can be used to put an upper bound on the number of query results in 
each database's query cache and thus restrict the cache's memory consumption.

The value can also be adjusted at runtime as follows:

```
require("@arangodb/aql/cache").properties({ maxResults: 200 }); 
```


!SECTION Per-query configuration

When a query is sent to the server for execution and the cache is set to `on` or `demand`,
the query executor will look into the query's `cache` attribute. If the query cache mode is
`on`, then not setting this attribute or setting it to anything but `false` will make the
query executor consult the query cache. If the query cache mode is `demand`, then setting
the `cache` attribute to `true` will make the executor look for the query in the query cache.
When the query cache mode is `off`, the executor will not look for the query in the cache.

The `cache` attribute can be set as follows via the `db._createStatement()` function:

```
var stmt = db._createStatement({ 
  query: "FOR doc IN users LIMIT 5 RETURN doc",
  cache: true  /* cache attribute set here */
}); 

stmt.execute();
```

When using the `db._query()` function, the `cache` attribute can be set as allows:

```
db._query({ 
  query: "FOR doc IN users LIMIT 5 RETURN doc",
  cache: true  /* cache attribute set here */
}); 
```

The `cache` attribute can be set via the HTTP REST API `POST /_api/cursor`, too.

Each query result returned will contain a `cached` attribute. This will be set to `true`
if the result was retrieved from the query cache, and `false` otherwise. Clients can use
this attribute to check if a specific query was served from the cache or not.


!SECTION Restrictions

Query results that are returned from the query cache do not contain any execution statistics,
meaning their *extra.stats* attribute will not be present. Additionally, query results returned
from the cache will not contain profile information even if the *profile* option was set to
true when invoking the query.

