
@startDocuBlock get_admin_metrics
@brief return the current instance metrics

@RESTHEADER{GET /_admin/metrics, Read the metrics, getMetrics}

@RESTDESCRIPTION
Returns the instance's current metrics in prometheus format. The
returned document collects all instance metrics, which are measured
at any given time and exposes them for collection by prometheus.

The document contains different metrics and metrics groups dependent
on the role of the queried instance. All exported metrics are
published with the `arangodb_` or `rocksdb_` string to identify the
from other collected data. 

The API then needs to be added to the prometheus configuration file
for collection.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Metrics were returned successfully.

@endDocuBlock

