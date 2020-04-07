
@startDocuBlock get_api_wal_access_tail
@brief Fetch recent operations

@RESTHEADER{GET /_api/wal/tail, Tail recent server operations, handleCommandTail}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{global,boolean,optional}
If set to *true*, tails the WAL for all databases. If set to *false*, tails the 
WAL for the selected database.

@RESTQUERYPARAM{from,number,optional}
Exclusive lower bound tick value for results. On successive calls
to this API you should set this to the value returned
with the *x-arango-replication-lastincluded* header (Unless that header
contains 0).

@RESTQUERYPARAM{to,number,optional}
Inclusive upper bound tick value for results.

@RESTQUERYPARAM{lastScanned,number,optional}
Should be set to the value of the *x-arango-replication-lastscanned* header
or alternatively 0 on first try. This allows the rocksdb engine to break up
large transactions over multiple responses.

@RESTQUERYPARAM{global,boolean,optional}
Whether operations for all databases should be included. When set to *false*
only the operations for the current database are included. The value *true* is
only valid on the *_system* database. The default is *false*.

@RESTQUERYPARAM{chunkSize,number,optional}
Approximate maximum size of the returned result.

@RESTQUERYPARAM{syncerId,number,optional}
Id of the client used to tail results. The server will use this to
keep operations until the client has fetched them. Must be a positive integer.
**Note** this or serverId is required to have a chance at fetching reading all
operations with the rocksdb storage engine.

@RESTQUERYPARAM{serverId,number,optional}
Id of the client machine. If *syncerId* is unset, the server will use
this to keep operations until the client has fetched them. Must be a positive
integer.
**Note** this or syncerId is required to have a chance at fetching reading all
operations with the rocksdb storage engine.

@RESTQUERYPARAM{clientInfo,string,optional}
Short description of the client, used for informative purposes only.

@RESTQUERYPARAM{barrierId,number,optional}
Id of barrier used to keep WAL entries around. **Note** this is only required for the
MMFiles storage engine

@RESTDESCRIPTION
Returns data from the server's write-ahead log (also named replication log). This method can be called
by replication clients after an initial synchronization of data. The method
will return all "recent" logged operations from the server. Clients
can replay and apply these operations locally so they get to the same data
state as the server.

Clients can call this method repeatedly to incrementally fetch all changes
from the server. In this case, they should provide the *from* value so
they will only get returned the log events since their last fetch.

When the *from* query parameter is not used, the server will return log
entries starting at the beginning of its replication log. When the *from*
parameter is used, the server will only return log entries which have
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

- *cuid*: globally unique id of the View or collection the event was for

- *db*: the database name the event was for

- *tid*: id of the transaction the event was contained in

- *data*: the original document data

A more detailed description of the individual replication event types and their
data structures can be found in [Operation Types](#operation-types).

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

- *x-arango-replication-lastscanned*: the last tick the server scanned while
  computing the operation log. This might include operations the server did not
  returned to you due to various reasons (i.e. the value was filtered or skipped).
  You may use this value in the *lastScanned* header to allow the rocksdb engine
  to break up requests over multiple responses.

- *x-arango-replication-lasttick*: the last tick value the server has
  logged in its write ahead log (not necessarily included in the result). By comparing the the last
  tick and last included tick values, clients have an approximate indication of
  how many events there are still left to fetch.

- *x-arango-replication-frompresent*: is set to _true_ if server returned
  all tick values starting from the specified tick in the _from_ parameter.
  Should this be set to false the server did not have these operations anymore
  and the client might have missed operations.

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

@EXAMPLE_ARANGOSH_RUN{RestWalAccessTailingEmpty}
    var re = require("@arangodb/replication");
    var lastTick = re.logger.state().state.lastLogTick;

    var url = "/_api/wal/tail?from=" + lastTick;
    var response = logCurlRequest('GET', url);

    assert(response.code === 204);

    logRawResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

A few log events *(One JSON document per line)*

@EXAMPLE_ARANGOSH_RUN{RestWalAccessTailingSome}
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
    var url = "/_api/wal/tail?from=" + lastTick;
    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonLResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

More events than would fit into the response

@EXAMPLE_ARANGOSH_RUN{RestWalAccessTailingBufferLimit}
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
    var url = "/_api/wal/tail?from=" + lastTick + "&chunkSize=400";
    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
