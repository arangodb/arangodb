
@startDocuBlock put_admin_compact
@brief compact all databases

@RESTHEADER{PUT /_admin/compact, Compact the entire database system data, RestCompactHandler}

@HINTS
{% hint 'warning' %}
This command can cause a full rewrite of all data in all databases, which may
take very long for large databases. It should thus only be used with care and
only when additional I/O load can be tolerated for a prolonged time.
{% endhint %}

@RESTDESCRIPTION
This endpoint can be used to reclaim disk space after substantial data
deletions have taken place. It requires superuser access.

@RESTBODYPARAM{changeLevel,boolean,optional,}
whether or not compacted data should be moved to the minimum possible level.
The default value is *false*.

@RESTBODYPARAM{compactBottomMostLevel,boolean,optional,}
Whether or not to compact the bottommost level of data.
The default value is *false*.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Compaction started successfully

@RESTRETURNCODE{401}
if the request was not authenticated as a user with sufficient rights

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestAdminCompact}
    var response = logCurlRequest('PUT', '/_admin/compact', '');

    assert(response.code === 200);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
