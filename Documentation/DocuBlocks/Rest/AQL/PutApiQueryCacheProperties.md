
@startDocuBlock PutApiQueryCacheProperties
@brief changes the configuration for the AQL query cache

@RESTHEADER{PUT /_api/query-cache/properties, Globally adjusts the AQL query result cache properties}

@RESTDESCRIPTION
After the properties have been changed, the current set of properties will
be returned in the HTTP response.

Note: changing the properties may invalidate all results in the cache.
The global properties for AQL query cache.
The properties need to be passed in the attribute *properties* in the body
of the HTTP request. *properties* needs to be a JSON object with the following
properties:

@RESTBODYPARAM{mode,string,required,string}
 the mode the AQL query cache should operate in. Possible values are *off*, *on* or *demand*.

@RESTBODYPARAM{maxResults,integer,required,int64}
the maximum number of query results that will be stored per database-specific cache.


@RESTRETURNCODES

@RESTRETURNCODE{200}
Is returned if the properties were changed successfully.

@RESTRETURNCODE{400}
The server will respond with *HTTP 400* in case of a malformed request,

@endDocuBlock

