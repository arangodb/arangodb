
@startDocuBlock delete_api_query_cache

@RESTHEADER{DELETE /_api/query-cache, Clear the AQL query results cache, deleteAqlQueryCache}

@RESTDESCRIPTION
Clears all results stored in the AQL query results cache for the current database.

@RESTRETURNCODES

@RESTRETURNCODE{200}
The server will respond with *HTTP 200* when the cache was cleared
successfully.

@RESTRETURNCODE{400}
The server will respond with *HTTP 400* in case of a malformed request.
@endDocuBlock
