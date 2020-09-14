
@startDocuBlock put_api_replication_makeFollower
@brief Changes role to follower

@RESTHEADER{PUT /_api/replication/make-follower, Turn the server into a follower of another, handleCommandMakeFollower}

@RESTBODYPARAM{endpoint,string,required,string}
the leader endpoint to connect to (e.g. "tcp://192.168.173.13:8529").

@RESTBODYPARAM{database,string,required,string}
the database name on the leader (if not specified, defaults to the
name of the local current database).

@RESTBODYPARAM{username,string,optional,string}
an optional ArangoDB username to use when connecting to the leader.

@RESTBODYPARAM{password,string,required,string}
the password to use when connecting to the leader.

@RESTBODYPARAM{includeSystem,boolean,required,}
whether or not system collection operations will be applied

@RESTBODYPARAM{restrictType,string,optional,string}
an optional string value for collection filtering. When
specified, the allowed values are *include* or *exclude*.

@RESTBODYPARAM{restrictCollections,array,optional,string}
an optional array of collections for use with *restrictType*.
If *restrictType* is *include*, only the specified collections
will be synchronized. If *restrictType* is *exclude*, all but the specified
collections will be synchronized.

@RESTBODYPARAM{maxConnectRetries,integer,optional,int64}
the maximum number of connection attempts the applier
will make in a row. If the applier cannot establish a connection to the
endpoint in this number of attempts, it will stop itself.

@RESTBODYPARAM{connectTimeout,integer,optional,int64}
the timeout (in seconds) when attempting to connect to the
endpoint. This value is used for each connection attempt.

@RESTBODYPARAM{requestTimeout,integer,optional,int64}
the timeout (in seconds) for individual requests to the endpoint.

@RESTBODYPARAM{chunkSize,integer,optional,int64}
the requested maximum size for log transfer packets that
is used when the endpoint is contacted.

@RESTBODYPARAM{adaptivePolling,boolean,optional,}
whether or not the replication applier will use adaptive polling.

@RESTBODYPARAM{autoResync,boolean,optional,}
whether or not the follower should perform an automatic resynchronization with
the leader in case the leader cannot serve log data requested by the follower,
or when the replication is started and no tick value can be found.

@RESTBODYPARAM{autoResyncRetries,integer,optional,int64}
number of resynchronization retries that will be performed in a row when
automatic resynchronization is enabled and kicks in. Setting this to *0* will
effectively disable *autoResync*. Setting it to some other value will limit
the number of retries that are performed. This helps preventing endless retries
in case resynchronizations always fail.

@RESTBODYPARAM{initialSyncMaxWaitTime,integer,optional,int64}
the maximum wait time (in seconds) that the initial synchronization will
wait for a response from the leader when fetching initial collection data.
This wait time can be used to control after what time the initial synchronization
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

@RESTBODYPARAM{requireFromPresent,boolean,optional,}
if set to *true*, then the replication applier will check
at start of its continuous replication if the start tick from the dump phase
is still present on the leader. If not, then there would be data loss. If
*requireFromPresent* is *true*, the replication applier will abort with an
appropriate error message. If set to *false*, then the replication applier will
still start, and ignore the data loss.

@RESTBODYPARAM{verbose,boolean,optional,}
if set to *true*, then a log line will be emitted for all operations
performed by the replication applier. This should be used for debugging
replication
problems only.

@RESTDESCRIPTION
Starts a full data synchronization from a remote endpoint into the local ArangoDB
database and afterwards starts the continuous replication.
The operation works on a per-database level.

All local database data will be removed prior to the synchronization.

In case of success, the body of the response is a JSON object with the following
attributes:

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
    provide.

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

WARNING: calling this method will synchronize data from the collections found
on the remote leader to the local ArangoDB database. All data in the local
collections will be purged and replaced with data from the leader.

Use with caution!

Please also keep in mind that this command may take a long time to complete
and return. This is because it will first do a full data synchronization with
the leader, which will take time roughly proportional to the amount of data.

**Note**: this method is not supported on a Coordinator in a cluster.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the request was executed successfully.

@RESTRETURNCODE{400}
is returned if the configuration is incomplete or malformed.

@RESTRETURNCODE{405}
is returned when an invalid HTTP method is used.

@RESTRETURNCODE{500}
is returned if an error occurred during synchronization or when starting the
continuous replication.

@RESTRETURNCODE{501}
is returned when this operation is called on a Coordinator in a cluster.
@endDocuBlock
