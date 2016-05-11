
@startDocuBlock GetApiQuerySlow
@brief returns a list of slow running AQL queries

@RESTHEADER{GET /_api/query/slow, Returns the list of slow AQL queries}

@RESTDESCRIPTION
Returns an array containing the last AQL queries that exceeded the slow
query threshold in the selected database.
The maximum amount of queries in the list can be controlled by setting
the query tracking property `maxSlowQueries`. The threshold for treating
a query as *slow* can be adjusted by setting the query tracking property
`slowQueryThreshold`.

Each query is a JSON object with the following attributes:

- *id*: the query's id

- *query*: the query string (potentially truncated)

- *started*: the date and time when the query was started

- *runTime*: the query's run time up to the point the list of queries was
  queried

@RESTRETURNCODES

@RESTRETURNCODE{200}
Is returned when the list of queries can be retrieved successfully.

@RESTRETURNCODE{400}
The server will respond with *HTTP 400* in case of a malformed request,

@endDocuBlock

