
@startDocuBlock DeleteApiQueryCache
@brief clears the AQL query cache

@RESTHEADER{DELETE /_api/query-cache, Clears any results in the AQL query cache}

@RESTDESCRIPTION
clears the query cache
@RESTRETURNCODES

@RESTRETURNCODE{200}
The server will respond with *HTTP 200* when the cache was cleared
successfully.

@RESTRETURNCODE{400}
The server will respond with *HTTP 400* in case of a malformed request.
@endDocuBlock

