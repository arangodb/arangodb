
@startDocuBlock GetApiQueryCacheProperties
@brief returns the global configuration for the AQL query results cache

@RESTHEADER{GET /_api/query-cache/properties, Returns the global properties for the AQL query results cache, readProperties}

@RESTDESCRIPTION
Returns the global AQL query results cache configuration. The configuration is a
JSON object with the following properties:

- *mode*: the mode the AQL query results cache operates in. The mode is one of the following
  values: *off*, *on* or *demand*.

- *maxResults*: the maximum number of query results that will be stored per database-specific
  cache.

- *maxResultsSize*: the maximum cumulated size of query results that will be stored per
  database-specific cache.

- *maxEntrySize*: the maximum individual result size of queries that will be stored per
  database-specific cache.

- *includeSystem*: whether or not results of queries that involve system collections will be
  stored in the query results cache.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Is returned if the properties can be retrieved successfully.

@RESTRETURNCODE{400}
The server will respond with *HTTP 400* in case of a malformed request,

@endDocuBlock
