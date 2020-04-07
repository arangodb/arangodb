
@startDocuBlock get_api_replication_logger_follow
@brief Fetch log lines from the server

@RESTHEADER{GET /_api/replication/logger-follow, Returns log entries, handleCommandLoggerFollow}

@HINTS
{% hint 'warning' %}
This route should no longer be used.
It is considered as deprecated from version 3.4.0 on.
{% endhint %}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{from,number,optional}
Exclusive lower bound tick value for results.

@RESTQUERYPARAM{to,number,optional}
Inclusive upper bound tick value for results.

@RESTQUERYPARAM{chunkSize,number,optional}
Approximate maximum size of the returned result.

@RESTQUERYPARAM{includeSystem,boolean,optional}
Include system collections in the result. The default value is *true*.

@RESTDESCRIPTION
Returns data from the server's replication log. This method can be called
by replication clients after an initial synchronization of data. The method
will return all "recent" log entries from the logger server, and the clients
can replay and apply these entries locally so they get to the same data
state as the logger server.

Clients can call this method repeatedly to incrementally fetch all changes
from the logger server. In this case, they should provide the *from* value so
they will only get returned the log events since their last fetch.

When the *from* query parameter is not used, the logger server will return log
entries starting at the beginning of its replication log. When the *from*
parameter is used, the logger server will only return log entries which have
higher tick values than the specified *from* value (note: the log entry with a
tick value equal to *from* will be excluded). Use the *from* value when
incrementally fetching log data.

The *to* query parameter can be used to optionally restrict the upper bound of
the result to a certain tick value. If used, the result will contain only log events
with tick values up to (including) *to*. In incremental fetching, there is no
need to use the *to* parameter. It only makes sense in special situations,
when only parts of the change log are required.

The *chunkSize* query parameter can be used to control the size of the result.
It must be specified in bytes. The *chunkSize* value will only be honored
approximately. Otherwise a too low *chunkSize* value could cause the server
to not be able to put just one log entry into the result and return it.
Therefore, the *chunkSize* value will only be consulted after a log entry has
been written into the result. If the result size is then bigger than
*chunkSize*, the server will respond with as many log entries as there are
in the response already. If the result size is still smaller than *chunkSize*,
the server will try to return more data if there's more data left to return.

If *chunkSize* is not specified, some server-side default value will be used.

The *Content-Type* of the result is *application/x-arango-dump*. This is an
easy-to-process format, with all log events going onto separate lines in the
response body. Each log event itself is a JSON object, with at least the
following attributes:

- *tick*: the log event tick value

- *type*: the log event type

Individual log events will also have additional attributes, depending on the
event type. A few common attributes which are used for multiple events types
are:

- *cid*: id of the collection the event was for

- *tid*: id of the transaction the event was contained in

- *key*: document key

- *rev*: document revision id

- *data*: the original document data

A more detailed description of the individual replication event types and their
data structures can be found in [Operation Types](../Replications/WALAccess.html#operation-types).

The response will also contain the following HTTP headers:

- *x-arango-replication-active*: whether or not the logger is active. Clients
  can use this flag as an indication for their polling frequency. If the
  logger is not active and there are no more replication events available, it
  might be sensible for a client to abort, or to go to sleep for a long time
  and try again later to check whether the logger has been activated.

- *x-arango-replication-lastincluded*: the tick value of the last included
  value in the result. In incremental log fetching, this value can be used
  as the *from* value for the following request. **Note** that if the result is
  empty, the value will be *0*. This value should not be used as *from* value
  by clients in the next request (otherwise the server would return the log
  events from the start of the log again).

- *x-arango-replication-lasttick*: the last tick value the logger server has
  logged (not necessarily included in the result). By comparing the the last
  tick and last included tick values, clients have an approximate indication of
  how many events there are still left to fetch.

- *x-arango-replication-checkmore*: whether or not there already exists more
  log data which the client could fetch immediately. If there is more log data
  available, the client could call *logger-follow* again with an adjusted *from*
  value to fetch remaining log entries until there are no more.

  If there isn't any more log data to fetch, the client might decide to go
  to sleep for a while before calling the logger again.

**Note**: this method is not supported on a Coordinator in a cluster.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the request was executed successfully, and there are log
events available for the requested range. The response body will not be empty
in this case.

@RESTRETURNCODE{204}
is returned if the request was executed successfully, but there are no log
events available for the requested range. The response body will be empty
in this case.

@RESTRETURNCODE{400}
is returned if either the *from* or *to* values are invalid.

@RESTRETURNCODE{405}
is returned when an invalid HTTP method is used.

@RESTRETURNCODE{500}
is returned if an error occurred while assembling the response.

@RESTRETURNCODE{501}
is returned when this operation is called on a Coordinator in a cluster.

@EXAMPLES

No log events available

@EXAMPLE_ARANGOSH_RUN{RestReplicationLoggerFollowEmpty}
    var re = require("@arangodb/replication");
    var lastTick = re.logger.state().state.lastLogTick;

    var url = "/_api/replication/logger-follow?from=" + lastTick;
    var response = logCurlRequest('GET', url);

    assert(response.code === 204);

    logRawResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

A few log events *(One JSON document per line)*

@EXAMPLE_ARANGOSH_RUN{RestReplicationLoggerFollowSome}
    var re = require("@arangodb/replication");
    db._drop("products");

    var lastTick = re.logger.state().state.lastLogTick;

    db._create("products");
    db.products.save({ "_key": "p1", "name" : "flux compensator" });
    db.products.save({ "_key": "p2", "name" : "hybrid hovercraft", "hp" : 5100 });
    db.products.remove("p1");
    db.products.update("p2", { "name" : "broken hovercraft" });
    db.products.drop();

    require("internal").wait(1);
    var url = "/_api/replication/logger-follow?from=" + lastTick;
    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonLResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

More events than would fit into the response

@EXAMPLE_ARANGOSH_RUN{RestReplicationLoggerFollowBufferLimit}
    var re = require("@arangodb/replication");
    db._drop("products");

    var lastTick = re.logger.state().state.lastLogTick;

    db._create("products");
    db.products.save({ "_key": "p1", "name" : "flux compensator" });
    db.products.save({ "_key": "p2", "name" : "hybrid hovercraft", "hp" : 5100 });
    db.products.remove("p1");
    db.products.update("p2", { "name" : "broken hovercraft" });
    db.products.drop();

    require("internal").wait(1);
    var url = "/_api/replication/logger-follow?from=" + lastTick + "&chunkSize=400";
    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
