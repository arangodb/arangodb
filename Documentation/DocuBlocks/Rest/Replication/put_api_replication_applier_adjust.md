
@startDocuBlock put_api_replication_applier_adjust
@brief set configuration values of an applier

@RESTHEADER{PUT /_api/replication/applier-config, Adjust configuration of replication applier,handleCommandApplierSetConfig}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{global,boolean,optional}
If set to *true*, adjusts the configuration of the global replication applier for all
databases. If set to *false*, adjusts the configuration of the replication applier in the
selected database.

@RESTBODYPARAM{endpoint,string,required,string}
the logger server to connect to (e.g. "tcp://192.168.173.13:8529"). The endpoint must be specified.

@RESTBODYPARAM{database,string,required,string}
the name of the database on the endpoint. If not specified, defaults to the current local database name.

@RESTBODYPARAM{username,string,optional,string}
an optional ArangoDB username to use when connecting to the endpoint.

@RESTBODYPARAM{password,string,required,string}
the password to use when connecting to the endpoint.

@RESTBODYPARAM{maxConnectRetries,integer,required,int64}
the maximum number of connection attempts the applier
will make in a row. If the applier cannot establish a connection to the
endpoint in this number of attempts, it will stop itself.

@RESTBODYPARAM{connectTimeout,integer,required,int64}
the timeout (in seconds) when attempting to connect to the
endpoint. This value is used for each connection attempt.

@RESTBODYPARAM{requestTimeout,integer,required,int64}
the timeout (in seconds) for individual requests to the endpoint.

@RESTBODYPARAM{chunkSize,integer,required,int64}
the requested maximum size for log transfer packets that
is used when the endpoint is contacted.

@RESTBODYPARAM{autoStart,boolean,required,}
whether or not to auto-start the replication applier on
(next and following) server starts

@RESTBODYPARAM{adaptivePolling,boolean,required,}
if set to *true*, the replication applier will fall
to sleep for an increasingly long period in case the logger server at the
endpoint does not have any more replication events to apply. Using
adaptive polling is thus useful to reduce the amount of work for both the
applier and the logger server for cases when there are only infrequent
changes. The downside is that when using adaptive polling, it might take
longer for the replication applier to detect that there are new replication
events on the logger server.<br>
Setting *adaptivePolling* to false will make the replication applier
contact the logger server in a constant interval, regardless of whether
the logger server provides updates frequently or seldom.

@RESTBODYPARAM{includeSystem,boolean,required,}
whether or not system collection operations will be applied

@RESTBODYPARAM{autoResync,boolean,optional,}
whether or not the follower should perform a full automatic resynchronization
with the leader in case the leader cannot serve log data requested by the
follower, or when the replication is started and no tick value can be found.

@RESTBODYPARAM{autoResyncRetries,integer,optional,int64}
number of resynchronization retries that will be performed in a row when
automatic resynchronization is enabled and kicks in. Setting this to *0*
will
effectively disable *autoResync*. Setting it to some other value will limit
the number of retries that are performed. This helps preventing endless
retries
in case resynchronizations always fail.

@RESTBODYPARAM{initialSyncMaxWaitTime,integer,optional,int64}
the maximum wait time (in seconds) that the initial synchronization will
wait for a response from the leader when fetching initial collection data.
This wait time can be used to control after what time the initial
synchronization
will give up waiting for a response and fail. This value is relevant even
for continuous replication when *autoResync* is set to *true* because this
may re-start the initial synchronization when the leader cannot provide
log data the follower requires.
This value will be ignored if set to *0*.

@RESTBODYPARAM{connectionRetryWaitTime,integer,optional,int64}
the time (in seconds) that the applier will intentionally idle before
it retries connecting to the leader in case of connection problems.
This value will be ignored if set to *0*.

@RESTBODYPARAM{idleMinWaitTime,integer,optional,int64}
the minimum wait time (in seconds) that the applier will intentionally idle
before fetching more log data from the leader in case the leader has
already sent all its log data. This wait time can be used to control the
frequency with which the replication applier sends HTTP log fetch requests
to the leader in case there is no write activity on the leader.
This value will be ignored if set to *0*.

@RESTBODYPARAM{idleMaxWaitTime,integer,optional,int64}
the maximum wait time (in seconds) that the applier will intentionally idle
before fetching more log data from the leader in case the leader has
already sent all its log data and there have been previous log fetch attempts
that resulted in no more log data. This wait time can be used to control the
maximum frequency with which the replication applier sends HTTP log fetch
requests to the leader in case there is no write activity on the leader for
longer periods. This configuration value will only be used if the option
*adaptivePolling* is set to *true*.
This value will be ignored if set to *0*.

@RESTBODYPARAM{requireFromPresent,boolean,required,}
if set to *true*, then the replication applier will check
at start whether the start tick from which it starts or resumes replication is
still present on the leader. If not, then there would be data loss. If
*requireFromPresent* is *true*, the replication applier will abort with an
appropriate error message. If set to *false*, then the replication applier will
still start, and ignore the data loss.

@RESTBODYPARAM{verbose,boolean,required,}
if set to *true*, then a log line will be emitted for all operations
performed by the replication applier. This should be used for debugging replication
problems only.

@RESTBODYPARAM{restrictType,string,required,string}
the configuration for *restrictCollections*; Has to be either *include* or *exclude*

@RESTBODYPARAM{restrictCollections,array,optional,string}
the array of collections to include or exclude,
based on the setting of *restrictType*

@RESTDESCRIPTION
Sets the configuration of the replication applier. The configuration can
only be changed while the applier is not running. The updated configuration
will be saved immediately but only become active with the next start of the
applier.

In case of success, the body of the response is a JSON object with the updated
configuration.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the request was executed successfully.

@RESTRETURNCODE{400}
is returned if the configuration is incomplete or malformed, or if the
replication applier is currently running.

@RESTRETURNCODE{405}
is returned when an invalid HTTP method is used.

@RESTRETURNCODE{500}
is returned if an error occurred while assembling the response.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestReplicationApplierSetConfig}
    var re = require("@arangodb/replication");
    re.applier.stop();

    var url = "/_api/replication/applier-config";
    var body = {
      endpoint: "tcp://127.0.0.1:8529",
      username: "replicationApplier",
      password: "applier1234@foxx",
      chunkSize: 4194304,
      autoStart: false,
      adaptivePolling: true
    };

    var response = logCurlRequest('PUT', url, body);

    assert(response.code === 200);
    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
