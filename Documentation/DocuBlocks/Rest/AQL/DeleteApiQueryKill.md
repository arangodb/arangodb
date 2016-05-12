
@startDocuBlock DeleteApiQueryKill
@brief kills an AQL query

@RESTHEADER{DELETE /_api/query/{query-id}, Kills a running AQL query}

@RESTURLPARAMETERS

@RESTURLPARAM{query-id,string,required}
The id of the query.

@RESTDESCRIPTION
Kills a running query. The query will be terminated at the next cancelation
point.

@RESTRETURNCODES

@RESTRETURNCODE{200}
The server will respond with *HTTP 200* when the query was still running when
the kill request was executed and the query's kill flag was set.

@RESTRETURNCODE{400}
The server will respond with *HTTP 400* in case of a malformed request.

@RESTRETURNCODE{404}
The server will respond with *HTTP 404* when no query with the specified
id was found.
@endDocuBlock

