WAL Access API
===========================

The WAL Access API is used from 3.3 onwards to facilitate faster and
more reliable asynchronous replication. The API offers access to the 
write-ahead log or operations log of the ArangoDB server. As a public
API it is only supported to access these REST endpoints on a single-server
instance. While these APIs are also available on DBServer instances, accessing them
as a user is not supported. This API replaces some of the APIs in `/_api/replication`.

<!-- arangod/RestHandler/RestWALHandler.cpp -->
@startDocuBlock get_api_wal_access_range

@startDocuBlock get_api_wal_access_last_tick

@startDocuBlock get_api_wal_access_tail

Operation Types
----------------

There are several different operation types thar an ArangoDB server might print. 
All operations include a `tick` value which identified their place in the operations log.
The numeric fields _tick_ and _tid_ always contain stringified numbers to avoid problems with 
drivers where numbers in JSON might be mishandled.

The following operation types are used in ArangoDB:

### Create Database (1100)

Create a database. Contains the field _db_ with the database name and the field _data_, 
contains the database definition.
```json
{
  "tick": "2103",
  "type": 1100,
  "db": "test",
  "data": {
    "database": 337,
    "id": "337",
    "name": "test"
  }
}
```

### Drop Database (1100)

Drop a database. Contains the field _db_ with the database name.
```json
{
  "tick": "3453",
  "type": 1101,
  "db": "test"
}
```

### Create Collection (2000)

Create a collection. Contains the field _db_ with the database name, and _cuid_ with the 
globally unique id to identify this collection. The *data* attribute contains the collection definition.

```json
{
  "tick": "3702",
  "db": "_system",
  "cuid": "hC0CF79DA83B4/555",
  "type": 2000,
  "data": {
    "allowUserKeys": true,
    "cacheEnabled": false,
    "cid": "555",
    "deleted": false,
    "globallyUniqueId": "hC0CF79DA83B4/555",
    "id": "555",
    "indexes": [],
    "isSystem": false,
    "keyOptions": {
      "allowUserKeys": true,
      "lastValue": 0,
      "type": "traditional"
    },
    "name": "test"
  }
}
```

### Drop Collection (2001)

Drop a collection. Contains the field _db_ with the database name, and _cuid_ with the 
globally unique id to identify this collection.

```json
{
  "tick": "154",
  "type": 2001,
  "db": "_system",
  "cuid": "hD15F8FE99859/555"
}
```

### Rename Collection (2002)

Rename a collection. Contains the field _db_ with the database name, and _cuid_ with the 
globally unique id to identify this collection. The _data_ field contains the *name* field
with the new name

```json
{
  "tick": "385",
  "db": "_system",
  "cuid": "hD15F8FE99859/135",
  "type": 2002,
  "data": {
    "name": "other"
  }
}
```

### Change Collection (2003)

Change collection properties. Contains the field _db_ with the database name, and _cuid_ with the 
globally unique id to identify this collection. The *data* attribute contains the updated collection definition.

```json
{
  "tick": "154",
  "type": 2003,
  "db": "_system",
  "cuid": "hD15F8FE99859/555",
  "data": {
    "waitForSync": true
  }
}
```

### Truncate Collection (2004)

Truncate a collection. Contains the field _db_ with the database name, and _cuid_ with the 
globally unique id to identify this collection.

```json
{
  "tick": "154",
  "type": 2004,
  "db": "_system",
  "cuid": "hD15F8FE99859/555"
}
```

### Create Index (2100)

Create an index. Contains the field _db_ with the database name, and _cuid_ with the 
globally unique id to identify this collection. The field _data_ contains the index
definition.

```json
{
  "tick": "1327",
  "type": 2100,
  "db": "_system",
  "cuid": "hD15F8FE99859/555",
  "data": {
    "deduplicate": true,
    "fields": [
      "value"
    ],
    "id": "260",
    "selectivityEstimate": 1,
    "sparse": false,
    "type": "skiplist",
    "unique": false
  }
}
```

### Drop Index (2101)

Drop an index. Contains the field _db_ with the database name, and _cuid_ with the 
globally unique id to identify this collection. The field _data_ contains the field
*id* with the index id.

```json
{
  "tick": "1522",
  "type": 2101,
  "db": "_system",
  "cuid": "hD15F8FE99859/555",
  "data": {
    "id": "260"
  }
}
```

### Create View (2110)

Create a view. Contains the field _db_ with the database name, and _cuid_ with the 
globally unique id to identify this view. The field _data_ contains the view definition

```json
{
  "tick": "1833",
  "type": 2110,
  "db": "_system",
  "cuid": "hD15F8FE99859/322",
  "data": {
    "cleanupIntervalStep": 10,
    "collections": [],
    "commitIntervalMsec": 60000,
    "consolidate": {
      "segmentThreshold": 300,
      "threshold": 0.8500000238418579,
      "type": "bytes_accum"
    },
    "deleted": false,
    "globallyUniqueId": "hD15F8FE99859/322",
    "id": "322",
    "isSystem": false,
    "locale": "C",
    "name": "myview",
    "type": "arangosearch"
  }
}
```

### Drop View (2111)

Drop a view. Contains the field _db_ with the database name, and _cuid_ with the 
globally unique id to identify this view.

```json
{
  "tick": "3113",
  "type": 2111,
  "db": "_system",
  "cuid": "hD15F8FE99859/322"
}
```

### Change View (2112)

Change view properties (including the name). Contains the field _db_ with the database name and _cuid_ with the 
globally unique id to identify this view. The *data* attribute contain the updated properties.

```json
{
  "tick": "3014",
  "type": 2112,
  "db": "_system",
  "cuid": "hD15F8FE99859/457",
  "data": {
    "cleanupIntervalStep": 10,
    "collections": [
      135
    ],
    "commitIntervalMsec": 60000,
    "consolidate": {
      "segmentThreshold": 300,
      "threshold": 0.8500000238418579,
      "type": "bytes_accum"
    },
    "deleted": false,
    "globallyUniqueId": "hD15F8FE99859/457",
    "id": "457",
    "isSystem": false,
    "locale": "C",
    "name": "renamedview",
    "type": "arangosearch"
  }
}
```

### Start Transaction (2200)

Mark the beginning of a transaction. Contains the field _db_ with the database name
and the field _tid_ for the transaction id. This log entry might be followed
by zero or more document operations and then either one commit **or** an abort operation 
(i.e. types *2300*, *2302* and *2201* / *2202*) with the same _tid_ value.

```json
{
  "tick": "3651",
  "type": 2200,
  "db": "_system",
  "tid": "556"
}
```

### Commit Transaction (2201)

Mark the successful end of a transaction. Contains the field _db_ with the database name
and the field _tid_ for the transaction id.

```json
{
  "tick": "3652",
  "type": 2201,
  "db": "_system",
  "tid": "556"
}
```

### Abort Transaction (2202)

Mark the abortion of a transaction. Contains the field _db_ with the database name
and the field _tid_ for the transaction id.

```json
{
  "tick": "3654",
  "type": 2202,
  "db": "_system",
  "tid": "556"
}
```

### Insert / Replace Document (2300)

Insert or replace a document. Contains the field _db_ with the database name,
_cuid_ with the globally unique id to identify the collection and the field _tid_ for 
the transaction id. The field *tid* might contain the value *"0"* to identify a single
operation that is not part of a multi-document transaction. The field *data* contains the
document. If the field *_rev* exists the client can choose to perform a revision check against
a locally available version of the document to ensure consistency.

```json
{
  "tick": "196",
  "type": 2300,
  "db": "_system",
  "tid": "0",
  "cuid": "hE0E3D7BE511D/119",
  "data": {
    "_id": "users/194",
    "_key": "194",
    "_rev": "_XUJFD3C---",
    "value": "test"
  }
}
```

### Remove Document (2302)

Remove a document. Contains the field _db_ with the database name,
_cuid_ with the globally unique id to identify the collection and the field _tid_ for 
the transaction id. The field *tid* might contain the value *"0"* to identify a single
operation that is not part of a multi-document transaction. The field *data* contains the 
*_key* and *_rev* of the removed document. The client can choose to perform a revision check against
a locally available version of the document to ensure consistency.

```json
{
  "cuid": "hE0E3D7BE511D/119",
  "data": {
    "_key": "194",
    "_rev": "_XUJIbS---_"
  },
  "db": "_system",
  "tick": "397",
  "tid": "0",
  "type": 2302
}
```
