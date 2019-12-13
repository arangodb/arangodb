
@startDocuBlock GetApiQueryProperties
@brief returns the configuration for the AQL query tracking

@RESTHEADER{GET /_api/query/properties, Returns the properties for the AQL query tracking, readQueryProperties}

@RESTDESCRIPTION
Returns the current query tracking configuration. The configuration is a
JSON object with the following properties:

- *enabled*: if set to *true*, then queries will be tracked. If set to
  *false*, neither queries nor slow queries will be tracked.

- *trackSlowQueries*: if set to *true*, then slow queries will be tracked
  in the list of slow queries if their runtime exceeds the value set in
  *slowQueryThreshold*. In order for slow queries to be tracked, the *enabled*
  property must also be set to *true*.

- *trackBindVars*: if set to *true*, then bind variables used in queries will
  be tracked.

- *maxSlowQueries*: the maximum number of slow queries to keep in the list
  of slow queries. If the list of slow queries is full, the oldest entry in
  it will be discarded when additional slow queries occur.

- *slowQueryThreshold*: the threshold value for treating a query as slow. A
  query with a runtime greater or equal to this threshold value will be
  put into the list of slow queries when slow query tracking is enabled.
  The value for *slowQueryThreshold* is specified in seconds.

- *maxQueryStringLength*: the maximum query string length to keep in the
  list of queries. Query strings can have arbitrary lengths, and this property
  can be used to save memory in case very long query strings are used. The
  value is specified in bytes.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Is returned if properties were retrieved successfully.

@RESTRETURNCODE{400}
The server will respond with *HTTP 400* in case of a malformed request,

@endDocuBlock
