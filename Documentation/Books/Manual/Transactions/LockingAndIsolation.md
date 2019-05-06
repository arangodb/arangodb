Locking and Isolation
=====================

Transactions need to specify from which collections they will read data and which
collections they intend to modify. This can be done by setting the *read*, *write*,
or *exclusive* attributes in the *collections* attribute of the transaction:

```js
db._executeTransaction({
  collections: { 
    read: "users",
    write: ["test", "log"]
  },
  action: function () {
    const db = require("@arangodb").db;
    db.users.toArray().forEach(function(doc) {
      db.log.insert({ value: "removed user: " + doc.name });
      db.test.remove(doc._key);
    });
  }
});
```

*write* here means write access to the collection, and also includes any read accesses. 
*exclusive* is a synonym for *write* in the MMFiles engine, because both *exclusive* and
*write* will acquire collection-level locks in this engine. In the RocksDB engine,
*exclusive* means exclusive write access to the collection, and *write* means (shared)
write access to the collection, which can be interleaved with write accesses by other
concurrent transactions.

MMFiles engine
--------------

The *MMFiles engine* uses the following locking mechanisms to serialize transactions
on the same data:

All collections specified in the *collections* attribute are locked in the
requested mode (read or write) at transaction start. Locking of multiple collections
is performed in alphabetical order.
When a transaction commits or rolls back, all locks are released in reverse order.
The locking order is deterministic to avoid deadlocks.

While locks are held, modifications by other transactions to the collections 
participating in the transaction are prevented.
A transaction will thus see a consistent view of the participating collections' data.

Additionally, a transaction will not be interrupted or interleaved with any other 
ongoing operations on the same collection. This means each transaction will run in
isolation. A transaction should never see uncommitted or rolled back modifications by
other transactions. Additionally, reads inside a transaction are repeatable.

Note that the above is true only for all collections that are declared in the 
*collections* attribute of the transaction.

RocksDB engine
--------------

The *RocksDB* engine does not lock any collections participating in a transaction
for read. Read operations can run in parallel to other read or write operations on the
same collections.


### Locking

For all collections that are used in write mode, the RocksDB engine will internally
acquire a (shared) read lock. This means that many writers can modify data in the same
collection in parallel (and also run in parallel to ongoing reads). However, if two
concurrent transactions attempt to modify the same document or index entry, there will
be a write-write conflict, and one of the transactions will abort with error 1200
("conflict"). It is then up to client applications to retry the failed transaction or 
accept the failure.

In order to guard long-running or complex transactions against concurrent operations
on the same data, the RocksDB engine allows to access collections in exclusive mode.
Exclusive accesses will internally acquire a write-lock on the collections, so they 
are not executed in parallel with any other write operations. Read operations can still
be carried out by other concurrent transactions.

### Isolation

The RocksDB storage-engine provides *snapshot isolation*. This means that all operations 
and queries in the transactions will see the same version, or snapshot, of the database. 
This snapshot is based on the state of the database at the moment in time when the transaction 
begins. No locks are acquired on the underlying data to keep this snapshot, which permits 
other transactions to execute without being blocked by an older uncompleted transaction 
(so long as they do not try to modify the same documents or unique index-entries concurrently).
In the cluster a snapshot is acquired on each DBServer individually. 

Lazily adding collections
-------------------------

There might be situations when declaring all collections a priori is not possible,
for example, because further collections are determined by a dynamic AQL query 
inside the transaction, for example a query using AQL graph traversal.

In this case, it would be impossible to know beforehand which collection to lock, and
thus it is legal to not declare collections that will be accessed in the transaction in
read-only mode. Accessing a non-declared collection in read-only mode during a 
transaction will add the collection to the transaction lazily, and fetch data 
from the collection as usual. However, as the collection is added lazily, there is no 
isolation from other concurrent operations or transactions. Reads from such
collections are potentially non-repeatable.

**Examples:**

```js
db._executeTransaction({
  collections: { 
    read: "users"
  },
  action: function () {
    const db = require("@arangodb").db;
    /* Execute an AQL query that traverses a graph starting at a "users" vertex.
       It is yet unknown into which other collections the query might traverse */
    db._createStatement({ 
      query: `FOR v IN ANY "users/1234" connections RETURN v`
    }).execute().toArray().forEach(function (d) {
      /* ... */
    });
  }
});
```

This automatic lazy addition of collections to a transaction also introduces the 
possibility of deadlocks. Deadlocks may occur if there are concurrent transactions 
that try to acquire locks on the same collections lazily.

In order to make a transaction fail when a non-declared collection is used inside
a transaction for reading, the optional *allowImplicit* sub-attribute of *collections* 
can be set to *false*:

```js
db._executeTransaction({
  collections: { 
    read: "users",
    allowImplicit: false
  },
  action: function () {
    /* The below query will now fail because the collection "connections" has not
       been specified in the list of collections used by the transaction */
    const db = require("@arangodb").db;
    db._createStatement({ 
      query: `FOR v IN ANY "users/1234" connections RETURN v`
    }).execute().toArray().forEach(function (d) {
      /* ... */
    });
  }
});
```

The default value for *allowImplicit* is *true*. Write-accessing collections that
have not been declared in the *collections* array is never possible, regardless of
the value of *allowImplicit*.

If *users/1234* has an edge in *connections*, linking it to another document in
the *users* collection, then the following explicit declaration will work:

```js
db._executeTransaction({
  collections: { 
    read: ["users", "connections"],
    allowImplicit: false
  },
  /* ... */
```

If the edge points to a document in another collection however, then the query
will fail, unless that other collection is added to the declaration as well.

Note that if a document handle is used as starting point for a traversal, e.g.
`FOR v IN ANY "users/not_linked" ...` or `FOR v IN ANY {_id: "users/not_linked"} ...`,
then no error is raised in the case of the start vertex not having any edges to
follow, with `allowImplicit: false` and *users* not being declared for read access.
AQL only sees a string and does not consider it a read access, unless there are
edges connected to it. `FOR v IN ANY DOCUMENT("users/not_linked") ...` will fail
even without edges, as it is always considered to be a read access to the *users*
collection.

Deadlocks and Deadlock detection
--------------------------------

A deadlock is a situation in which two or more concurrent operations (user transactions
or AQL queries) try to access the same resources (collections, documents) and need to 
wait for the others to finish, but none of them can make any progress.

A good example for a deadlock is two concurrently executing transactions T1 and T2 that
try to access the same collections but that need to wait for each other. In this example,
transaction T1 will write to collection `c1`, but will also read documents from
collection `c2` without announcing it:

```js
db._executeTransaction({
  collections: { 
    write: "c1"
  },
  action: function () {
    const db = require("@arangodb").db;

    /* write into c1 (announced) */
    db.c1.insert({ foo: "bar" });

    /* some operation here that takes long to execute... */

    /* read from c2 (unannounced) */
    db.c2.toArray();
  }
});
```

Transaction T2 announces to write into collection `c2`, but will also read 
documents from collection `c1` without announcing it:

```js
db._executeTransaction({
  collections: { 
    write: "c2"
  },
  action: function () {
    var db = require("@arangodb").db;

    /* write into c2 (announced) */
    db.c2.insert({ bar: "baz" });
    
    /* some operation here that takes long to execute... */
    
    /* read from c1 (unannounced) */
    db.c1.toArray();
  }
});
```

In the above example, a deadlock will occur if transaction T1 and T2 have both
acquired their write locks (T1 for collection `c1` and T2 for collection `c2`) and
are then trying to read from the other other (T1 will read from `c2`, T2 will read
from `c1`). T1 will then try to acquire the read lock on collection `c2`, which
is prevented by transaction T2. T2 however will wait for the read lock on 
collection `c1`, which is prevented by transaction T1. 

In case of such deadlock, there would be no progress for any of the involved 
transactions, and none of the involved transactions could ever complete. This is
completely undesirable, so the automatic deadlock detection mechanism in ArangoDB
will automatically abort one of the transactions involved in such deadlock. Aborting
means that all changes done by the transaction will be rolled back and error 29 
(`deadlock detected`) will be thrown. 

Client code (AQL queries, user transactions) that accesses more than one collection 
should be aware of the potential of deadlocks and should handle the error 29 
(`deadlock detected`) properly, either by passing the exception to the caller or 
retrying the operation.

To avoid both deadlocks and non-repeatable reads, all collections used in a 
transaction should be specified in the `collections` attribute when known in advance.
In case this is not possible because collections are added dynamically inside the
transaction, deadlocks may occur and the deadlock detection may kick in and abort
the transaction. 

The *RocksDB* engine uses document-level locks and therefore will not have a deadlock
problem on collection level. If two concurrent transactions however modify the same
documents or index entries, the RocksDB engine will signal a write-write conflict
and abort one of the transactions with error 1200 ("conflict") automatically.
