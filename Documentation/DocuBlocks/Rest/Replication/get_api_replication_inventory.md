
@startDocuBlock get_api_replication_inventory

@RESTHEADER{GET /_api/replication/inventory, Get a replication inventory, getReplicationInventory}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{includeSystem,boolean,optional}
Include system collections in the result. The default value is `true`.

@RESTQUERYPARAM{global,boolean,optional}
Include all databases in the response. Only works on `_system` The default value is `false`.

@RESTQUERYPARAM{batchId,number,required}
A valid batchId is required for this API call

@RESTQUERYPARAM{collection,string,optional}
If this parameter is set, the response will be restricted to a single collection (the one
specified), and no views will be returned. This can be used as an optimization to reduce
the size of the response.

@RESTDESCRIPTION
Returns the array of collections and their indexes, and the array of Views available. These
arrays can be used by replication clients to initiate an initial synchronization with the
server. 
The response will contain all collections, their indexes and views in the requested database
if `global` is not set, and all collections, indexes and views in all databases if `global`
is set.
In case `global` is not set, it is possible to restrict the response to a single collection
by setting the `collection` parameter. In this case the response will contain only information
about the requested collection in the `collections` array, and no information about views
(i.e. the `views` response attribute will be an empty array).

The response will contain a JSON object with the `collections`, `views`, `state` and
`tick` attributes.

`collections` is an array of collections with the following sub-attributes:

- `parameters`: the collection properties

- `indexes`: an array of the indexes of a the collection. Primary indexes and edge indexes
   are not included in this array.

The `state` attribute contains the current state of the replication logger. It
contains the following sub-attributes:

- `running`: whether or not the replication logger is currently active. Note:
  since ArangoDB 2.2, the value will always be `true`

- `lastLogTick`: the value of the last tick the replication logger has written

- `time`: the current time on the server

`views` is an array of available views.

Replication clients should note the `lastLogTick` value returned. They can then
fetch collections' data using the dump method up to the value of lastLogTick, and
query the continuous replication log for log events after this tick value.

To create a full copy of the collections on the server, a replication client
can execute these steps:

- call the `/inventory` API method. This returns the `lastLogTick` value and the
  array of collections and indexes from the server.

- for each collection returned by `/inventory`, create the collection locally and
  call `/dump` to stream the collection data to the client, up to the value of
  `lastLogTick`.
  After that, the client can create the indexes on the collections as they were
  reported by `/inventory`.

If the clients wants to continuously stream replication log events from the logger
server, the following additional steps need to be carried out:

- the client should call `/_api/wal/tail` initially to fetch the first batch of
  replication events that were logged after the client's call to `/inventory`.

  The call to `/_api/wal/tail` should use a `from` parameter with the value of the
  `lastLogTick` as reported by `/inventory`. The call to `/_api/wal/tail` will
  return the `x-arango-replication-lastincluded` header which will contain the
  last tick value included in the response.

- the client can then continuously call `/_api/wal/tail` to incrementally fetch new
  replication events that occurred after the last transfer.

  Calls should use a `from` parameter with the value of the `x-arango-replication-lastincluded`
  header of the previous response. If there are no more replication events, the
  response will be empty and clients can go to sleep for a while and try again
  later.

**Note**: on a Coordinator, this request must have a `DBserver`
query parameter which must be an ID of a DB-Server.
The very same request is forwarded synchronously to that DB-Server.
It is an error if this attribute is not bound in the Coordinator case.

**Note**: Using the `global` parameter the top-level object contains a key `databases`
under which each key represents a database name, and the value conforms to the above description.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the request was executed successfully.

@RESTRETURNCODE{405}
is returned when an invalid HTTP method is used.

@RESTRETURNCODE{500}
is returned if an error occurred while assembling the response.

<!-- TODO How to find out the RocksDB batchId?
 EXAMPLES

 EXAMPLE_ARANGOSH_RUN{RestReplicationInventory_mmfiles}
    var url = "/_api/replication/inventory";
    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
 END_EXAMPLE_ARANGOSH_RUN

With some additional indexes:

 EXAMPLE_ARANGOSH_RUN{RestReplicationInventoryIndexes_mmfiles}
    db._drop("IndexedCollection1");
    var c1 = db._create("IndexedCollection1");
    c1.ensureIndex({ type: "persistent", fields: ["name"] });
    c1.ensureIndex({ type: "persistent", fields: ["a", "b"], unique: true });

    db._drop("IndexedCollection2");
    var c2 = db._create("IndexedCollection2");
    c2.ensureIndex({ type: "fulltext", fields: ["text"], minLength: 10 });
    c2.ensureIndex({ type: "persistent", fields: ["a"] });

    var url = "/_api/replication/inventory";
    var response = logCurlRequest('GET', url);

    assert(response.code === 200);
    logJsonResponse(response);

    db._flushCache();
    db._drop("IndexedCollection1");
    db._drop("IndexedCollection2");
 END_EXAMPLE_ARANGOSH_RUN
-->
@endDocuBlock
