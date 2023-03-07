
@startDocuBlock PutApiQueryProperties
@brief changes the configuration for the AQL query tracking

@RESTHEADER{PUT /_api/query/properties, Changes the properties for the AQL query tracking, replaceProperties}

@RESTBODYPARAM{enabled,boolean,required,}
If set to *true*, then queries will be tracked. If set to
*false*, neither queries nor slow queries will be tracked.

@RESTBODYPARAM{trackSlowQueries,boolean,required,}
If set to *true*, then slow queries will be tracked
in the list of slow queries if their runtime exceeds the value set in
*slowQueryThreshold*. In order for slow queries to be tracked, the *enabled*
property must also be set to *true*.

@RESTBODYPARAM{trackBindVars,boolean,required,}
If set to *true*, then the bind variables used in queries will be tracked
along with queries.

@RESTBODYPARAM{maxSlowQueries,integer,required,int64}
The maximum number of slow queries to keep in the list
of slow queries. If the list of slow queries is full, the oldest entry in
it will be discarded when additional slow queries occur.

@RESTBODYPARAM{slowQueryThreshold,integer,required,int64}
The threshold value for treating a query as slow. A
query with a runtime greater or equal to this threshold value will be
put into the list of slow queries when slow query tracking is enabled.
The value for *slowQueryThreshold* is specified in seconds.

@RESTBODYPARAM{maxQueryStringLength,integer,required,int64}
The maximum query string length to keep in the list of queries.
Query strings can have arbitrary lengths, and this property
can be used to save memory in case very long query strings are used. The
value is specified in bytes.

@RESTDESCRIPTION
The properties need to be passed in the attribute *properties* in the body
of the HTTP request. *properties* needs to be a JSON object.

After the properties have been changed, the current set of properties will
be returned in the HTTP response.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Is returned if the properties were changed successfully.

@RESTRETURNCODE{400}
The server will respond with *HTTP 400* in case of a malformed request,

@endDocuBlock
