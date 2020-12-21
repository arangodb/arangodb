
@startDocuBlock GetApiQuerySlow
@brief returns a list of slow running AQL queries

@RESTHEADER{GET /_api/query/slow, Returns the list of slow AQL queries, readQuery:Slow}

@RESTDESCRIPTION
Returns an array containing the last AQL queries that are finished and
have exceeded the slow query threshold in the selected database.
The maximum amount of queries in the list can be controlled by setting
the query tracking property `maxSlowQueries`. The threshold for treating
a query as *slow* can be adjusted by setting the query tracking property
`slowQueryThreshold`.

Each query is a JSON object with the following attributes:

- *id*: the query's id

- *database*: the name of the database the query runs in

- *user*: the name of the user that started the query

- *query*: the query string (potentially truncated)

- *bindVars*: the bind parameter values used by the query

- *started*: the date and time when the query was started

- *runTime*: the query's total run time

- *state*: the query's current execution state (will always be "finished"
  for the list of slow queries)

- *stream*: whether or not the query uses a streaming cursor

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{all,boolean,optional}
If set to *true*, will return the slow queries from all databases, not just
the selected one.
Using the parameter is only allowed in the system database and with superuser
privileges.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Is returned when the list of queries can be retrieved successfully.

@RESTRETURNCODE{400}
The server will respond with *HTTP 400* in case of a malformed request,

@RESTRETURNCODE{403}
*HTTP 403* is returned in case the *all* parameter was used, but the request
was made in a different database than _system, or by an non-privileged user.

@endDocuBlock
