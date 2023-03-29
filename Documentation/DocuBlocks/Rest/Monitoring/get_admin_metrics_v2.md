
@startDocuBlock get_admin_metrics_v2
@brief return the current instance metrics

@RESTHEADER{GET /_admin/metrics/v2, Read the metrics, getMetricsV2}

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
published with the prefix `arangodb_` or `rocksdb_` to distinguish them from
other collected data.

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

@EXAMPLE_ARANGOSH_RUN{RestAdminMetricsV2}
    var url = "/_admin/metrics/v2";
    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logPlainResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
