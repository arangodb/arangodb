
@startDocuBlock get_admin_metrics

@RESTHEADER{GET /_admin/metrics, Get the metrics (deprecated), getMetrics}

@HINTS
{% hint 'warning' %}
This endpoint should no longer be used. It is deprecated from version 3.8.0 on.
Use `/_admin/metrics/v2` instead. From version 3.10.0 onward, `/_admin/metrics`
returns the same metrics as `/_admin/metrics/v2`.
{% endhint %}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{serverId,string,optional}
Returns metrics of the specified server. If no serverId is given, the asked 
server will reply. This parameter is only meaningful on Coordinators.

@RESTDESCRIPTION
Returns the instance's current metrics in Prometheus format. The
returned document collects all instance metrics, which are measured
at any given time and exposes them for collection by Prometheus.

The document contains different metrics and metrics groups dependent
on the role of the queried instance. All exported metrics are
published with the `arangodb_` or `rocksdb_` string to distinguish
them from other collected data. 

The API then needs to be added to the Prometheus configuration file
for collection.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Metrics were returned successfully.

@RESTRETURNCODE{404}
The metrics API may be disabled using `--server.export-metrics-api false`
setting in the server. In this case, the result of the call indicates the API
to be not found.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestAdminMetrics}
    var url = "/_admin/metrics";
    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logPlainResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
