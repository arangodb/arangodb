
@startDocuBlock GetApiQueryCacheProperties
@brief returns the global configuration for the AQL query cache

@RESTHEADER{GET /_api/query-cache/properties, Returns the global properties for the AQL query cache}

@RESTDESCRIPTION
Returns the global AQL query cache configuration. The configuration is a
JSON object with the following properties:

- *mode*: the mode the AQL query cache operates in. The mode is one of the following
  values: *off*, *on* or *demand*.

- *maxResults*: the maximum number of query results that will be stored per database-specific
  cache.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Is returned if the properties can be retrieved successfully.

@RESTRETURNCODE{400}
The server will respond with *HTTP 400* in case of a malformed request,

@endDocuBlock

