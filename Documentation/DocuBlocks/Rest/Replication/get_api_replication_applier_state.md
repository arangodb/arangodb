
@startDocuBlock get_api_replication_applier_state
@brief output the current status of the replication

@RESTHEADER{GET /_api/replication/applier-state, State of the replication applier,handleCommandApplierGetState}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{global,boolean,optional}
If set to *true*, returns the state of the global replication applier for all
databases. If set to *false*, returns the state of the replication applier in the
selected database.

@RESTDESCRIPTION
Returns the state of the replication applier, regardless of whether the
applier is currently running or not.

The response is a JSON object with the following attributes:

- *state*: a JSON object with the following sub-attributes:

  - *running*: whether or not the applier is active and running

  - *lastAppliedContinuousTick*: the last tick value from the continuous
    replication log the applier has applied.

  - *lastProcessedContinuousTick*: the last tick value from the continuous
    replication log the applier has processed.

    Regularly, the last applied and last processed tick values should be
    identical. For transactional operations, the replication applier will first
    process incoming log events before applying them, so the processed tick
    value might be higher than the applied tick value. This will be the case
    until the applier encounters the *transaction commit* log event for the
    transaction.

  - *lastAvailableContinuousTick*: the last tick value the remote server can
    provide, for all databases.

  - *ticksBehind*: this attribute will be present only if the applier is currently
    running. It will provide the number of log ticks between what the applier
    has applied/seen and the last log tick value provided by the remote server.
    If this value is zero, then both servers are in sync. If this is non-zero,
    then the remote server has additional data that the applier has not yet
    fetched and processed, or the remote server may have more data that is not
    applicable to the applier.

    Client applications can use it to determine approximately how far the applier
    is behind the remote server, and can periodically check if the value is
    increasing (applier is falling behind) or decreasing (applier is catching up).

    Please note that as the remote server will only keep one last log tick value
    for all of its databases, but replication may be restricted to just certain
    databases on the applier, this value is more meaningful when the global applier
    is used.
    Additionally, the last log tick provided by the remote server may increase
    due to writes into system collections that are not replicated due to replication
    configuration. So the reported value may exaggerate the reality a bit for
    some scenarios.

  - *time*: the time on the applier server.

  - *totalRequests*: the total number of requests the applier has made to the
    endpoint.

  - *totalFailedConnects*: the total number of failed connection attempts the
    applier has made.

  - *totalEvents*: the total number of log events the applier has processed.

  - *totalOperationsExcluded*: the total number of log events excluded because
    of *restrictCollections*.

  - *progress*: a JSON object with details about the replication applier progress.
    It contains the following sub-attributes if there is progress to report:

    - *message*: a textual description of the progress

    - *time*: the date and time the progress was logged

    - *failedConnects*: the current number of failed connection attempts

  - *lastError*: a JSON object with details about the last error that happened on
    the applier. It contains the following sub-attributes if there was an error:

    - *errorNum*: a numerical error code

    - *errorMessage*: a textual error description

    - *time*: the date and time the error occurred

    In case no error has occurred, *lastError* will be empty.

- *server*: a JSON object with the following sub-attributes:

  - *version*: the applier server's version

  - *serverId*: the applier server's id

- *endpoint*: the endpoint the applier is connected to (if applier is
  active) or will connect to (if applier is currently inactive)

- *database*: the name of the database the applier is connected to (if applier is
  active) or will connect to (if applier is currently inactive)

Please note that all "tick" values returned do not have a specific unit. Tick
values are only meaningful when compared to each other. Higher tick values mean
"later in time" than lower tick values.
@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the request was executed successfully.

@RESTRETURNCODE{405}
is returned when an invalid HTTP method is used.

@RESTRETURNCODE{500}
is returned if an error occurred while assembling the response.

@EXAMPLES

Fetching the state of an inactive applier:

@EXAMPLE_ARANGOSH_RUN{RestReplicationApplierStateNotRunning}
    var re = require("@arangodb/replication");
    re.applier.stop();

    var url = "/_api/replication/applier-state";
    var response = logCurlRequest('GET', url);

    assert(response.code === 200);
    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Fetching the state of an active applier:

@EXAMPLE_ARANGOSH_RUN{RestReplicationApplierStateRunning}
    var re = require("@arangodb/replication");
    re.applier.stop();
    re.applier.start();

    var url = "/_api/replication/applier-state";
    var response = logCurlRequest('GET', url);
    re.applier.stop();

    assert(response.code === 200);
    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
