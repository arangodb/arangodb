
@startDocuBlock GetApiQueryCacheCurrent
@brief returns a list of the stored results in the AQL query results cache

@RESTHEADER{GET /_api/query-cache/entries, Returns the currently cached query results, readQueries}

@RESTDESCRIPTION
Returns an array containing the AQL query results currently stored in the query results
cache of the selected database. Each result is a JSON object with the following attributes:

- *hash*: the query result's hash

- *query*: the query string

- *bindVars*: the query's bind parameters. this attribute is only shown if tracking for
  bind variables was enabled at server start

- *size*: the size of the query result and bind parameters, in bytes

- *results*: number of documents/rows in the query result

- *started*: the date and time when the query was stored in the cache

- *hits*: number of times the result was served from the cache (can be
  *0* for queries that were only stored in the cache but were never accessed
  again afterwards)

- *runTime*: the query's run time

- *dataSources*: an array of collections/Views the query was using

@RESTRETURNCODES

@RESTRETURNCODE{200}
Is returned when the list of results can be retrieved successfully.

@RESTRETURNCODE{400}
The server will respond with *HTTP 400* in case of a malformed request,

@endDocuBlock
