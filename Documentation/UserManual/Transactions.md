Transactions {#Transactions}
============================

@NAVIGATE_Transactions
@EMBEDTOC{TransactionsTOC}


Introduction {#TransactionsIntroduction}
========================================

Starting with version 1.3, ArangoDB provides support for user-definable 
transactions. 

Transactions in ArangoDB are atomic, consistent, isolated, and durable (*ACID*).

These *ACID* properties provide the following guarantees:
- The *atomicity* priniciple makes transactions either complete in their
  entirety or have no effect at all.
- The *consistency* principle ensures that no constraints or other invariants
  will be violated during or after any transaction.
- The *isolation* property will hide the modifications of a transaction from
  other transactions until the transaction commits. 
- Finally, the *durability* proposition makes sure that operations from 
  transactions that have committed will be made persistent. The amount of
  transaction durability is configurable in ArangoDB, as is the durability
  on collection level.


Transaction invocation {#TransactionsInvocation}
================================================

ArangoDB transactions are different from transactions in SQL.

In SQL, transactions are started with explicit `BEGIN` or `START TRANSACTION`
command. Following any series of data retrieval or modification operations, an
SQL transaction is finished with a `COMMIT` command, or rolled back with a
`ROLLBACK` command. There may be client/server communication between the start
and the commit/rollback of an SQL transaction.

In ArangoDB, a transaction is always a server-only operation. A transaction
is executed on the server in one go, without any client interaction. All 
operations to be executed inside a transaction need to be known by the server
when the transaction is started.

There are no individual `BEGIN`, `COMMIT` or `ROLLBACK` commands in ArangoDB.
Instead, a transaction in ArangoDB is started by providing a description of the
transaction to the `TRANSACTION` Javascript function:

    TRANSACTION(description);

This function will then automatically start a transaction, execute all required
data retrieval and/or modification operations, and at the end automatically 
commit the transaction. If an error occurs during transaction execution, the
transaction is automatically rolled back.


Declaration of collections
==========================

All collections which are to participate in a transaction need to be declared 
beforehand. This is a necessity to ensure proper locking and isolation. 

Collections can be used in a transaction in write mode or in read-only mode.

If any data modification operations are to be executed, the collection must be 
used in write mode. The write mode allows modifying and reading data from the 
collection during the transaction (thus, the write mode includes the read mode,
too).

Contrary, using a collection in read-only mode will only allow performing 
read operations on a collection. Any attempt to write into a collection used
in read-only mode will make the transaction fail.

Collections for a transaction are declared using the `collections` attribute of
the object passed to the `TRANSACTION` function:

    TRANSACTION({
      collections: {
        write: [ "users", "logins" ],
        read: [ "recommendations" ],
      },
      ...
    });

`read` and `write` are optional attributes, and only need to be specified if 
the operations inside the transactions demand for it.

The contents of `read` or `write` can be lists of collections names or a 
single collection name (as a string):
    
    TRANSACTION({
      collections: {
        write: "users",
        read: "recommendations",
      },
      ...
    });


Note that it is currently optional to specify collections for read-only access.
Even without specifying them, it is still possible to read from such collections
from within a transaction, but with relaxed isolation. Please refer to 
@ref TransactionsLocking for more details.


Declaration of data modification and retrieval operations
=========================================================

All data modification and retrieval operations that are to be executed inside
the transaction need to be specified in a Javascript function, using the `action`
attribute:

    TRANSACTION({
      collections: {
        write: "users",
      },
      action: function () {
        /* all operations go here */
      }
    });

Any valid Javascript code is allowed inside `action` but the code may only
access the collections declared in `collections`.

When the code inside the `action` attribute is executed, the transaction is
already started and all required locks are held by the transaction.
When the code inside the `action` attribute finishes, the transaction will
either commit or rollback, depending on the return value of the function.

If `true` is returned, the transaction will commit. If any other value is
returned from the `action`, then the transaction will roll back. If an exception
is thrown insdie the `action` code and is not caught, the transaction will also
roll back.

    TRANSACTION({
      collections: {
        write: "users",
      },
      action: function () {
        db.users.save({ _key: "hello" });

        /* will commit the transaction */
        return true;
      }
    });

Examples
========

The first example will write 3 documents into a collection named `c1`. 
The `c1` collection needs to be declared in the `write` attribute of the 
`collections` attribute passed to the `TRANSACTION` function.

The `action` attribute contains the actual transaction code to be executed.
This code contains all data modification operations (3 in this example).
The `return true` at the end of the code will make the transaction commit.

    /* setup */
    var db = require("internal").db;
    db._create("c1");

    TRANSACTION({
      collections: {
        write: [ "c1" ]
      },
      action: function () {
        db.c1.save({ _key: "key1" });
        db.c1.save({ _key: "key2" });
        db.c1.save({ _key: "key3" });
 
        /* will cause a commit */
        return true; 
      }
    });

    db.c1.count(); /* 3 */


Failure to return `true` from the `TRANSACTION` function will make the transaction 
roll back automatically:
    
    /* setup */
    var db = require("internal").db;
    db._create("c1");

    TRANSACTION({
      collections: {
        write: [ "c1" ]
      },
      action: function () {
        db.c1.save({ _key: "key1" });
        db.c1.count(); /* 1 */

        db.c1.save({ _key: "key2" });
        db.c1.count(); /* 2 */

        /* return true missing here, so transaction will not commit */
      }
    });

    db.c1.count(); /* 0 */


The automatic rollback is also executed when an exception is thrown at some
point during transaction execution:

    /* setup */
    var db = require("internal").db;
    db._create("c1");

    TRANSACTION({
      collections: {
        write: [ "c1" ]
      },
      action: function () {
        db.c1.save({ _key: "key1" });
        
        /* will throw duplicate a key error */
        db.c1.save({ _key: "key1" });  

        /* the return true will not be reached */
        return true; 
      }
    });

    db.c1.count(); /* 0 */


As required by the *consistency* principle, aborting or rolling back a 
transaction should also restore secondary indexes to the state at transaction
start. The following example using a cap constraint should illustrate that:

    /* setup */
    var db = require("internal").db;
    db._create("c1");
    
    /* limit the number of documents to 3 */
    db.c1.ensureCapConstraint(3); 

    /* insert 3 documents */
    db.c1.save({ _key: "key1" });
    db.c1.save({ _key: "key2" });
    db.c1.save({ _key: "key3" });

    /* this will push out key1 */
    /* we now have these keys: [ "key1", "key2", "key3" ] */
    db.c1.save({ _key: "key4" });


    TRANSACTION({
      collections: {
        write: [ "c1" ]
      },
      action: function () {
        /* this will push out key2. we now have keys [ "key3", "key4", "key5" ] */
        db.c1.save({ _key: "key5" }); 

        /* will abort the transaction */
        throw "doh!"
      }
    });


    /* we now have these keys back: [ "key2", "key3", "key4" ] */


Cross-collection transactions
=============================

There's also the possibility to run a transaction across multiple collections. 
In this case, multiple collections need to be declared in the `collections`
attribute, e.g.:

    /* setup */
    var db = require("internal").db;
    db._create("c1");
    db._create("c2");

    TRANSACTION({
      collections: {
        write: [ "c1", "c2" ]
      },
      action: function () {
        db.c1.save({ _key: "key1" });
        db.c2.save({ _key: "key2" });

        return true;
      }
    });

    db.c1.count(); /* 1 */
    db.c2.count(); /* 1 */


Again, not returning `true` from the `TRANSACTION` function will make the transaction 
roll back automatically, reverting the changes done in all declared collections:

    /* setup */
    var db = require("internal").db;
    db._create("c1");
    db._create("c2");

    TRANSACTION({
      collections: {
        write: [ "c1", "c2" ]
      },
      action: function () {
        for (var i = 0; i < 100; ++i) {
          db.c1.save({ _key: "key" + i });
          db.c2.save({ _key: "key" + i });
        }

        db.c1.count(); /* 100 */
        db.c2.count(); /* 100 */

        /* abort */
        throw "doh!"
      }
    });

    db.c1.count(); /* 0 */
    db.c2.count(); /* 0 */


Disallowed operations {#TransactionsDisallowedOperations}
=========================================================

Some operations are not allowed inside ArangoDB transactions:
- creation and deletion of collections (`db._create()`, `db._drop()`, `db._rename()`)
- creation and deletion of indexes (`db.ensure...Index()`, `db.dropIndex()`)

If an attempt is made to carry out any of these operations during a transaction,
ArangoDB will abort the transaction with error code `1655 (disallowed operation inside
transaction)`.


Locking and isolation {#TransactionsLocking}
============================================

All collections specified in the `collections` attribute are locked in the
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
`collections` attribute of the transaction.

There might be situations when declaring all collections a priori is not possible,
for example, because further collections are determined by a dynamic AQL query 
inside the transaction.
In this case, it would be impossible to know beforehand which collection to lock, and
thus it is legal to not declare collections that will be accessed in the transaction in
read-only mode. Accessing a non-declared collection in read-only mode during a 
transaction will add the collection to the transaction lazily, and fetch data 
from the collection as usual. However, as the collection ie added lazily, there is no 
isolation from other concurrent operations or transactions. Reads from such
collections are potentially non-repeatable.

Example:

    TRANSACTION({
      collections: { 
        read: "users"
      },
      action: function () {
        /* execute an AQL query that traverses a graph starting at a "users" vertex. 
           it is yet unknown into which other collections the query will traverse */
        db._createStatement({ 
          query: "FOR t IN TRAVERSAL(users, connections, "users/1234", "any", { }) RETURN t"
        }).execute().toArray().forEach(function (d) {
          /* ... */
        });
      }
    });


This automatic lazy addition of collections to a transaction also introduces the 
possibility of deadlocks. Deadlocks may occur if there are concurrent transactions 
that try to acquire locks on the same collections lazily.

To recover from a deadlock state, ArangoDB will give up waiting for a collection
after a configurable amount of time. The wait time can be specified per transaction 
using the optional`lockTimeout`attribute. If no value is specified, some default
value will be applied.

If ArangoDB was waited at least `lockTimeout` seconds during lock acquisition, it
will give up and rollback the transaction. Note that the `lockTimeout` is used per
lock acquisition in a transaction, and not just once per transaction. There will be 
at least as many lock acquisition attempts as there are collections used in the 
transaction. The total lock wait time may thus be much higher than the value of
`lockTimeout`.


To avoid both deadlocks and non-repeatable reads, all collections used in a 
transaction should always be specified if known in advance.


Durability {#TransactionsDurability}
====================================

Transactions are executed in main memory first until there is either a rollback
or a commit. On rollback, no data will be written to disk, but the operations 
from the transaction will be reversed in memory.

On commit, all modifications done in the transaction will be written to the 
collection datafiles. These writes will be synchronised to disk if any of the
modified collections has the `waitForSync` property set to `true`, or if any
individual operation in the transaction was executed with the `waitForSync` 
attribute. 
Additionally, transactions that modify data in more than one collection are
automatically synchronised to disk. This synchronisation is done to not only
ensure durability, but to also ensure consistency in case of a server crash.

That means if you only modify data in a single collection, and that collection 
has its `waitForSync` property set to `false`, the whole transaction will not 
be synchronised to disk instantly, but with a small delay.

There is thus the potential risk of losing data between the commit of the 
transaction and the actual (delayed) disk synchronisation. This is the same as 
writing into collections that have the `waitForSync` property set to `false`
outside of a transaction.
In case of a crash with `waitForSync` set to false, the operations performed in
the transaction will either be visible completely or not at all, depending on
whether the delayed synchronisation had kicked in or not.

To ensure durability of transactions on a collection that have the `waitForSync`
property set to `false`, you can set the `waitForSync` attribute of the object
that is passed to `TRANSACTION`. This will force a synchronisation of the
transaction to disk even for collections that have `waitForSync´ set to `false`:

    TRANSACTION({
      collections: { 
        write: "users"
      },
      waitForSync: true,
      action: function () { ... }
    });


An alternative is to perform an operation with an explicit `sync` request in
a transaction, e.g.

    db.users.save({ _key: "1234" }, true); 

In this case, the `true` value will make the whole transaction be synchronised
to disk at the commit.

In any case, ArangoDB will give users the choice of whether or not they want 
full durability for single collection transactions. Using the delayed synchronisation
(i.e. `waitForSync` with a value of `false`) will potentially increase throughput 
and performance of transactions, but will introduce the risk of losing the last
committed transactions in the case of a crash.

In contrast, transactions that modify data in more than one collection are 
automatically synchronised to disk. This comes at the cost of several disk sync
For a multi-collection transaction, the call to the `TRANSACTION` function will
only return only after the data of all modified collections has been synchronised 
to disk and the transaction has been made fully durable. This not only reduces the
risk of losing data in case of a crash but also ensures consistency after a
restart.

In case of a server crash, any multi-collection transactions that were not yet 
committed or in preparation to be committed will be rolled back on server restart.

For multi-collection transactions, there will be at least one disk sync operation 
per modified collection. Multi-collection transactions thus have a potentially higher
cost than single collection transactions. There is no configuration to turn off disk 
synchronisation for multi-collection transactions in ArangoDB. 
The disk sync speed of the system will thus be the most important factor for the 
performance of multi-collection transactions.


Limitations {#TransactionsLimitations}
======================================

Transactions in ArangoDB have been designed with particular use cases 
in mind. They will be mainly useful for short and small data retrieval 
and/or modification operations.

The implementation is not optimised for very long-running or very voluminuous
operations, and may not be usable for these cases. 

A major limitation is that a transaction must entirely fit into main
memory. This includes all data that is created, updated, or deleted during a
transaction, plus management overhead.

Transactions should thus be kept as small as possible, and big operations
should be split into multiple smaller transactions if they are too big to fit
into one transaction.

Additionally, transactions in ArangoDB cannot be nested, i.e. a transaction 
must not call any other transaction. If an attempt is made to call a transaction 
from inside a running transaction, the server will throw error `1652 (nested 
transactions detected`).

Finally, all collections that may be modified during a transaction must be 
declared beforehand, i.e. using the `collections` attribute of the object passed
to the `TRANSACTION` function. If any attempt is made to carry out a data
modification operation on a collection that was not declared in the `collections`
attribute, the transaction will be aborted and ArangoDB will throw error `1654
unregistered collection used in transaction`. 
It is legal to not declare read-only collections, but this should be avoided if
possible to reduce the probability of deadlocks and non-repeatable reads.

