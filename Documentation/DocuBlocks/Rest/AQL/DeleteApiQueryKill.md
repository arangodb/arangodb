
@startDocuBlock DeleteApiQueryKill
@brief kills an AQL query

@RESTHEADER{DELETE /_api/query/{query-id}, Kills a running AQL query, deleteQuery}

@RESTURLPARAMETERS

@RESTURLPARAM{query-id,string,required}
The id of the query.

@RESTDESCRIPTION
Kills a running query in the currently selected database. The query will be 
terminated at the next cancelation point.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{all,boolean,optional}
If set to *true*, will attempt to kill the specified query in all databases, 
not just the selected one.
Using the parameter is only allowed in the system database and with superuser
privileges.

@RESTRETURNCODES

@RESTRETURNCODE{200}
The server will respond with *HTTP 200* when the query was still running when
the kill request was executed and the query's kill flag was set.

@RESTRETURNCODE{400}
The server will respond with *HTTP 400* in case of a malformed request.

@RESTRETURNCODE{403}
*HTTP 403* is returned in case the *all* parameter was used, but the request
was made in a different database than _system, or by an non-privileged user.

@RESTRETURNCODE{404}
The server will respond with *HTTP 404* when no query with the specified
id was found.
@endDocuBlock
