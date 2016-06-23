

@brief maximum number of elements in the query cache per database
`--query.cache-entries`

Maximum number of query results that can be stored per database-specific
query cache. If a query is eligible for caching and the number of items in
the database's query cache is equal to this threshold value, another
cached
query result will be removed from the cache.

This option only has an effect if the query cache mode is set to either
*on* or *demand*.

