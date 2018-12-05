Incompatible changes in ArangoDB 2.6
====================================

It is recommended to check the following list of incompatible changes **before** 
upgrading to ArangoDB 2.6, and adjust any client programs if necessary.

Requirements
------------

ArangoDB's built-in web interface now uses cookies for session management.
Session information ids are stored in cookies, so clients using the web interface must 
accept cookies in order to log in and use it.

Foxx changes
------------

### Foxx Queues

Foxx Queue job type definitions were previously based on functions and had to be registered before use. Due to changes in 2.5 this resulted in problems when restarting the server or defining job types incorrectly.

Function-based job types have been deprecated in 2.6 and will be removed entirely in 2.7.

In order to convert existing function-based job types to the new script-based job types, create custom scripts in your Foxx app and reference them by their name and the mount point of the app they are defined in. Official job types from the Foxx app store can be upgraded by upgrading from the 1.x version to the 2.x version of the same app.

In order to upgrade queued jobs to the new job types, you need to update the `type` property of the affected jobs in the database's `_jobs` system collection. In order to see the collection in the web interface you need to enable the collection type "System" in the collection list options.

Example:

Before: `"type": "mailer.postmark"`

After: `"type": {"name": "mailer", "mount": "/my-postmark-mailer"}`

### Foxx Sessions

The options `jwt` and `type` of the controller method `controller.activateSessions` have been deprecated in 2.6 and will be removed entirely in 2.7.

If you want to use pure JWT sessions, you can use the `sessions-jwt` Foxx app from the Foxx app store.

If you want to use your own JWT-based sessions, you can use the JWT functions in the `crypto` module directly.

Instead of using the `type` option you can just use the `cookie` and `header` options on their own, which both now accept the value `true` to enable them with their default configurations.

The option `sessionStorageApp` has been renamed to `sessionStorage` and now also accepts session storages directly. The old option `sessionStorageApp` will be removed entirely in 2.7.

### Libraries

The bundled version of the `joi` library used in Foxx was upgraded to version 6.0.8.
This may affect Foxx applications that depend on the library.

AQL changes
-----------

### AQL LENGTH function

The return value of the AQL `LENGTH` function was changed if `LENGTH` is applied on `null` or a
boolean value:

* `LENGTH(null)` now returns `0`. In previous versions of ArangoDB, this returned `4`.

* `LENGTH(false)` now returns `0`. In previous versions of ArangoDB, the return value was `5`.

* `LENGTH(true)` now returns `1`. In previous versions of ArangoDB, the return value was `4`.

### AQL graph functions

In 2.6 the graph functions did undergo a performance lifting.
During this process we had to adopt the result format and the options for some of them.
Many graph functions now have an option `includeData` which allows to trigger
if the result of this function should contain fully extracted documents `includeData: true`
or only the `_id` values `includeData: false`.
In most use cases the `_id` is sufficient to continue and the extraction of data is an unnecessary
operation.
The AQL functions supporting this additional option are:

* SHORTEST_PATH
* NEIGHBORS
* GRAPH_SHORTEST_PATH
* GRAPH_NEIGHBORS
* GRAPH_EDGES

Furthermore the result `SHORTEST_PATH` has changed. The old format returned a list of all vertices on the path.
Optionally it could include each sub-path for these vertices.
All of the documents were fully extracted.
Example:
```
[
  {
    vertex: {
      _id: "vertex/1",
      _key: "1",
      _rev: "1234"
      name: "Alice"
    },
    path: {
      vertices: [
        {
          _id: "vertex/1",
          _key: "1",
          _rev: "1234"
          name: "Alice"
        }
      ],
      edges: []
    }
  },
  {
    vertex: {
      _id: "vertex/2",
      _key: "2",
      _rev: "5678"
      name: "Bob"
    },
    path: {
      vertices: [
        {
          _id: "vertex/1",
          _key: "1",
          _rev: "1234"
          name: "Alice"
        }, {
          _id: "vertex/2",
          _key: "2",
          _rev: "5678"
          name: "Bob"
        }
      ],
      edges: [
        {
          _id: "edge/1",
          _key: "1",
          _rev: "9876",
          type: "loves"
        }
      ]
    }
  }
]
```

The new version is more compact.
Each `SHORTEST_PATH` will only return one document having the attributes `vertices`, `edges`, `distance`.
The `distance` is computed taking into account the given weight.
Optionally the documents can be extracted with `includeData: true`
Example:
```
{
  vertices: [
    "vertex/1",
    "vertex/2"
  ],
  edges: [
    "edge/1"
  ],
  distance: 1
}
```

The next function that returns a different format is `NEIGHBORS`.
Since 2.5 it returned an object with `edge` and `vertex` for each connected edge.
Example:
```
[
  {
    vertex: {
      _id: "vertex/2",
      _key: "2",
      _rev: "5678"
      name: "Bob"
    },
    edge: {
      _id: "edge/1",
      _key: "1",
      _rev: "9876",
      type: "loves"
    }
  } 
]
```
With 2.6 it will only return the vertex directly, again using `includeData: true`.
By default it will return a distinct set of neighbors, using the option `distinct: false` 
will include the same vertex for each edge pointing to it.

Example:
```
[
  "vertex/2"
]
```

Function and API changes
------------------------

### Graph measurements functions

All graph measurements functions in JavaScript module `general-graph` that calculated a 
single figure previously returned an array containing just the figure. Now these functions 
will return the figure directly and not put it inside an array.

The affected functions are:

* `graph._absoluteEccentricity`
* `graph._eccentricity`
* `graph._absoluteCloseness`
* `graph._closeness`
* `graph._absoluteBetweenness`
* `graph._betweenness`
* `graph._radius`
* `graph._diameter`

Client programs calling these functions should be adjusted so they process the scalar value
returned by the function instead of the previous array value.

### Cursor API

A batchSize value `0` is now disallowed when calling the cursor API via HTTP 
`POST /_api/cursor`.

The HTTP REST API `POST /_api/cursor` does not accept a `batchSize` parameter value of 
`0` any longer. A batch size of 0 never made much sense, but previous versions of ArangoDB
did not check for this value. Now creating a cursor using a `batchSize` value 0 will
result in an HTTP 400 error response.

### Document URLs returned

The REST API method GET `/_api/document?collection=...` (that method will return partial URLs 
to all documents in the collection) will now properly prefix document address URLs with the 
current database name.

Previous versions of ArangoDB returned the URLs starting with `/_api/` but without the current 
database name, e.g. `/_api/document/mycollection/mykey`. Starting with 2.6, the response URLs
will include the database name as well, e.g. `/_db/_system/_api/document/mycollection/mykey`.

### Fulltext indexing

Fulltext indexes will now also index text values contained in direct sub-objects of the indexed 
attribute.

Previous versions of ArangoDB only indexed the attribute value if it was a string. Sub-attributes
of the index attribute were ignored when fulltext indexing.

Now, if the index attribute value is an object, the object's values will each be included in the
fulltext index if they are strings. If the index attribute value is an array, the array's values
will each be included in the fulltext index if they are strings.

Deprecated server functionality
-------------------------------

### Simple queries

The following simple query functions are now deprecated:

* `collection.near`
* `collection.within`
* `collection.geo`
* `collection.fulltext`
* `collection.range`
* `collection.closedRange`

This also lead to the following REST API methods being deprecated from now on:

* `PUT /_api/simple/near`
* `PUT /_api/simple/within`
* `PUT /_api/simple/fulltext`
* `PUT /_api/simple/range`

It is recommended to replace calls to these functions or APIs with equivalent AQL queries, 
which are more flexible because they can be combined with other operations:

    FOR doc IN NEAR(@@collection, @latitude, @longitude, @limit) 
      RETURN doc

    FOR doc IN WITHIN(@@collection, @latitude, @longitude, @radius, @distanceAttributeName)
      RETURN doc

    FOR doc IN FULLTEXT(@@collection, @attributeName, @queryString, @limit) 
      RETURN doc
  
    FOR doc IN @@collection 
      FILTER doc.value >= @left && doc.value < @right 
      LIMIT @skip, @limit 
      RETURN doc`
  
The above simple query functions and REST API methods may be removed in future versions 
of ArangoDB.

Using negative values for `SimpleQuery.skip()` is also deprecated. 
This functionality will be removed in future versions of ArangoDB.

### AQL functions

The AQL `SKIPLIST` function has been deprecated because it is obsolete.

The function was introduced in older versions of ArangoDB with a less powerful query optimizer to
retrieve data from a skiplist index using a `LIMIT` clause.

Since 2.3 the same goal can be achieved by using regular AQL constructs, e.g. 

    FOR doc IN @@collection 
      FILTER doc.value >= @value 
      SORT doc.value
      LIMIT 1 
      RETURN doc


Startup option changes
----------------------

### Options added

The following configuration options have been added in 2.6:

* `--server.session-timeout`: allows controlling the timeout of user sessions in the web interface.
  The value is specified in seconds.

* `--server.foxx-queues`: controls whether the Foxx queue manager will check queue and job entries.
  Disabling this option can reduce server load but will prevent jobs added to Foxx queues from
  being processed at all.

  The default value is `true`, enabling the Foxx queues feature.

* `--server.foxx-queues-poll-interval`: allows adjusting the frequency with which the Foxx queues 
  manager is checking the queue (or queues) for jobs to be executed.

  The default value is `1` second. Lowering this value will result in the queue manager waking
  up and checking the queues more frequently, which may increase CPU usage of the server. 
  
  Note: this option only has an effect when `--server.foxx-queues` is not set to `false`.

### Options removed

The following configuration options have been removed in 2.6.:

* `--log.severity`: the docs for `--log.severity` mentioned lots of severities (e.g. 
  `exception`, `technical`, `functional`, `development`) but only a few severities (e.g. 
  `all`, `human`) were actually used, with `human` being the default and `all` enabling the 
  additional logging of incoming requests. 
  
  The option pretended to control a lot of things which it actually didn't. Additionally,
  the option `--log.requests-file` was around for a long time already, also controlling 
  request logging. 

  Because the `--log.severity` option effectively did not control that much, it was removed. 
  A side effect of removing the option is that 2.5 installations started with option
  `--log.severity all` will not log requests after the upgrade to 2.6. This can be adjusted 
  by setting the `--log.requests-file` option instead.

### Default values changed

The default values for the following options have changed in 2.6:

* `--database.ignore-datafile-errors`: the default value for this option was changed from `true` 
  to `false`.

  If the new default value of `false` is used, then arangod will refuse loading collections that 
  contain datafiles with CRC mismatches or other errors. A collection with datafile errors will 
  then become unavailable. This prevents follow up errors from happening.
  
  The only way to access such collection is to use the datafile debugger (arango-dfdb) and try to 
  repair or truncate the datafile with it.

* `--server.request-timeout`: the default value was increased from 300 to 1200 seconds for all
  client tools (arangosh, arangoimp, arangodump, arangorestore).

* `--server.connect-timeout`: the default value was increased from 3 to 5 seconds for all client 
  tools (arangosh, arangoimp, arangodump, arangorestore).
