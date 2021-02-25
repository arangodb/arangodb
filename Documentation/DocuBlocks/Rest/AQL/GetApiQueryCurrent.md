
@startDocuBlock GetApiQueryCurrent
@brief returns a list of currently running AQL queries

@RESTHEADER{GET /_api/query/current, Returns the currently running AQL queries, readQuery:current}

@RESTDESCRIPTION
Returns an array containing the AQL queries currently running in the selected
database. Each query is a JSON object with the following attributes:

- *id*: the query's id

- *database*: the name of the database the query runs in

- *user*: the name of the user that started the query

- *query*: the query string (potentially truncated)

- *bindVars*: the bind parameter values used by the query

- *started*: the date and time when the query was started

- *runTime*: the query's run time up to the point the list of queries was
  queried

- *state*: the query's current execution state (as a string)

- *stream*: whether or not the query uses a streaming cursor

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{all,boolean,optional}
If set to *true*, will return the currently running queries in all databases,
not just the selected one.
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
