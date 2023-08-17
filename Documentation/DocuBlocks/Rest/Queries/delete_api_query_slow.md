
@startDocuBlock delete_api_query_slow

@RESTHEADER{DELETE /_api/query/slow, Clear the list of slow AQL queries, clearSlowAqlQueryList}

@RESTDESCRIPTION
Clears the list of slow AQL queries for the current database.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{all,boolean,optional}
If set to `true`, will clear the slow query history in all databases, not just
the selected one.
Using the parameter is only allowed in the system database and with superuser
privileges.

@RESTRETURNCODES

@RESTRETURNCODE{200}
The server will respond with *HTTP 200* when the list of queries was
cleared successfully.

@RESTRETURNCODE{400}
The server will respond with *HTTP 400* in case of a malformed request.
@endDocuBlock

@RESTRETURNCODE{403}
*HTTP 403* is returned in case the `all` parameter was used, but the request
was made in a different database than _system, or by an non-privileged user.
