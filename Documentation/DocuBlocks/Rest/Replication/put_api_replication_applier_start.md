
@startDocuBlock put_api_replication_applier_start
@brief start the replication applier

@RESTHEADER{PUT /_api/replication/applier-start, Start replication applier,handleCommandApplierStart}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{global,boolean,optional}
If set to *true*, starts the global replication applier for all
databases. If set to *false*, starts the replication applier in the
selected database.

@RESTQUERYPARAM{from,string,optional}
The remote *lastLogTick* value from which to start applying. If not specified,
the last saved tick from the previous applier run is used. If there is no
previous applier state saved, the applier will start at the beginning of the
logger server's log.

@RESTDESCRIPTION
Starts the replication applier. This will return immediately if the
replication applier is already running.

If the replication applier is not already running, the applier configuration
will be checked, and if it is complete, the applier will be started in a
background thread. This means that even if the applier will encounter any
errors while running, they will not be reported in the response to this
method.

To detect replication applier errors after the applier was started, use the
*/_api/replication/applier-state* API instead.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the request was executed successfully.

@RESTRETURNCODE{400}
is returned if the replication applier is not fully configured or the
configuration is invalid.

@RESTRETURNCODE{405}
is returned when an invalid HTTP method is used.

@RESTRETURNCODE{500}
is returned if an error occurred while assembling the response.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestReplicationApplierStart}
    var re = require("@arangodb/replication");
    re.applier.stop();
    re.applier.properties({
      endpoint: "tcp://127.0.0.1:8529",
      username: "replicationApplier",
      password: "applier1234@foxx",
      autoStart: false,
      adaptivePolling: true
    });

    var url = "/_api/replication/applier-start";
    var response = logCurlRequest('PUT', url, "");
    re.applier.stop();

    assert(response.code === 200);
    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
