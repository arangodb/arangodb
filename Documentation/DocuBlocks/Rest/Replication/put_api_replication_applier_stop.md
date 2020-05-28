
@startDocuBlock put_api_replication_applier_stop
@brief stop the replication

@RESTHEADER{PUT /_api/replication/applier-stop, Stop replication applier,handleCommandApplierStop}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{global,boolean,optional}
If set to *true*, stops the global replication applier for all
databases. If set to *false*, stops the replication applier in the
selected database.

@RESTDESCRIPTION
Stops the replication applier. This will return immediately if the
replication applier is not running.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the request was executed successfully.

@RESTRETURNCODE{405}
is returned when an invalid HTTP method is used.

@RESTRETURNCODE{500}
is returned if an error occurred while assembling the response.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestReplicationApplierStop}
    var re = require("@arangodb/replication");
    re.applier.stop();
    re.applier.properties({
      endpoint: "tcp://127.0.0.1:8529",
      username: "replicationApplier",
      password: "applier1234@foxx",
      autoStart: false,
      adaptivePolling: true
    });

    re.applier.start();
    var url = "/_api/replication/applier-stop";
    var response = logCurlRequest('PUT', url, "");
    re.applier.stop();

    assert(response.code === 200);
    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
