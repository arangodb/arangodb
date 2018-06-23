Transaction invocation
======================

ArangoDB transactions are different from transactions in SQL.

In SQL, transactions are started with explicit *BEGIN* or *START TRANSACTION*
command. Following any series of data retrieval or modification operations, an
SQL transaction is finished with a *COMMIT* command, or rolled back with a
*ROLLBACK* command. There may be client/server communication between the start
and the commit/rollback of an SQL transaction.

In ArangoDB, a transaction is always a server-side operation, and is executed
on the server in one go, without any client interaction. All operations to be 
executed inside a transaction need to be known by the server when the transaction 
is started.

There are no individual *BEGIN*, *COMMIT* or *ROLLBACK* transaction commands 
in ArangoDB. Instead, a transaction in ArangoDB is started by providing a 
description of the transaction to the *db._executeTransaction* JavaScript 
function:

```js
db._executeTransaction(description);
```

This function will then automatically start a transaction, execute all required
data retrieval and/or modification operations, and at the end automatically 
commit the transaction. If an error occurs during transaction execution, the
transaction is automatically aborted, and all changes are rolled back.

### Execute transaction
<!-- js/server/modules/@arangodb/arango-database.js -->


executes a transaction
`db._executeTransaction(object)`

Executes a server-side transaction, as specified by *object*.

*object* must have the following attributes:
- *collections*: a sub-object that defines which collections will be
  used in the transaction. *collections* can have these attributes:
  - *read*: a single collection or a list of collections that will be
    used in the transaction in read-only mode
  - *write*: a single collection or a list of collections that will be
    used in the transaction in write or read mode.
- *action*: a Javascript function or a string with Javascript code
  containing all the instructions to be executed inside the transaction.
  If the code runs through successfully, the transaction will be committed
  at the end. If the code throws an exception, the transaction will be
  rolled back and all database operations will be rolled back.

Additionally, *object* can have the following optional attributes:
- *waitForSync*: boolean flag indicating whether the transaction
  is forced to be synchronous.
- *lockTimeout*: a numeric value that can be used to set a timeout for
  waiting on collection locks. If not specified, a default value will be
  used. Setting *lockTimeout* to *0* will make ArangoDB not time
  out waiting for a lock.
- *params*: optional arguments passed to the function specified in
  *action*.



### Declaration of collections

All collections which are to participate in a transaction need to be declared 
beforehand. This is a necessity to ensure proper locking and isolation. 

Collections can be used in a transaction in write mode or in read-only mode.

If any data modification operations are to be executed, the collection must be 
declared for use in write mode. The write mode allows modifying and reading data 
from the collection during the transaction (i.e. the write mode includes the 
read mode).

Contrary, using a collection in read-only mode will only allow performing 
read operations on a collection. Any attempt to write into a collection used
in read-only mode will make the transaction fail.

Collections for a transaction are declared by providing them in the *collections* 
attribute of the object passed to the *_executeTransaction* function. The 
*collections* attribute has the sub-attributes *read* and *write*:

```js
db._executeTransaction({
  collections: {
    write: [ "users", "logins" ],
    read: [ "recommendations" ]
  }
});
```

*read* and *write* are optional attributes, and only need to be specified if 
the operations inside the transactions demand for it.

The contents of *read* or *write* can each be lists arrays collection names or a 
single collection name (as a string):

```js
db._executeTransaction({
  collections: {
    write: "users",
    read: "recommendations"
  }
});
```

**Note**: It is currently optional to specify collections for read-only access.
Even without specifying them, it is still possible to read from such collections
from within a transaction, but with relaxed isolation. Please refer to 
[Transactions Locking](../Transactions/LockingAndIsolation.md) for more details.

In order to make a transaction fail when a non-declared collection is used inside
for reading, the optional *allowImplicit* sub-attribute of *collections* can be
set to *false*:

```js
db._executeTransaction({
  collections: {
    read: "recommendations",
    allowImplicit: false  /* this disallows read access to other collections
                             than specified */
  },
  action: function () {
    var db = require("@arangodb").db;
    return db.foobar.toArray(); /* will fail because db.foobar must not be accessed
                                   for reading inside this transaction */
  }
});
```

The default value for *allowImplicit* is *true*. Write-accessing collections that
have not been declared in the *collections* array is never possible, regardless of
the value of *allowImplicit*.

### Declaration of data modification and retrieval operations

All data modification and retrieval operations that are to be executed inside
the transaction need to be specified in a Javascript function, using the *action*
attribute:

```js
db._executeTransaction({
  collections: {
    write: "users"
  },
  action: function () {
    // all operations go here 
  }
});
```

Any valid Javascript code is allowed inside *action* but the code may only
access the collections declared in *collections*.
*action* may be a Javascript function as shown above, or a string representation 
of a Javascript function:

```
db._executeTransaction({
  collections: {
    write: "users"
  },
  action: "function () { doSomething(); }"
});
```
Please note that any operations specified in *action* will be executed on the 
server, in a separate scope. Variables will be bound late. Accessing any JavaScript 
variables defined on the client-side or in some other server context from inside
a transaction may not work. 
Instead, any variables used inside *action* should be defined inside *action* itself:

```
db._executeTransaction({
  collections: {
    write: "users"
  },
  action: function () {
    var db = require(...).db;
    db.users.save({ ... });
  }
});
```

When the code inside the *action* attribute is executed, the transaction is
already started and all required locks have been acquired. When the code inside 
the *action* attribute finishes, the transaction will automatically commit.
There is no explicit commit command. 

To make a transaction abort and roll back all changes, an exception needs to
be thrown and not caught inside the transaction:

```js
db._executeTransaction({
  collections: {
    write: "users"
  },
  action: function () {
    var db = require("@arangodb").db;
    db.users.save({ _key: "hello" });
    // will abort and roll back the transaction 
    throw "doh!";
  }
});
```

There is no explicit abort or roll back command.

As mentioned earlier, a transaction will commit automatically when the end of
the *action* function is reached and no exception has been thrown. In this 
case, the user can return any legal JavaScript value from the function:

```js
db._executeTransaction({
  collections: {
    write: "users"
  },
  action: function () {
    var db = require("@arangodb").db;
    db.users.save({ _key: "hello" });
    // will commit the transaction and return the value "hello" 
    return "hello"; 
  }
});
```

### Examples

The first example will write 3 documents into a collection named *c1*. 
The *c1* collection needs to be declared in the *write* attribute of the 
*collections* attribute passed to the *executeTransaction* function.

The *action* attribute contains the actual transaction code to be executed.
This code contains all data modification operations (3 in this example).

```js
// setup
db._create("c1");

db._executeTransaction({
  collections: {
    write: [ "c1" ]
  },
  action: function () {
    var db = require("@arangodb").db;
    db.c1.save({ _key: "key1" });
    db.c1.save({ _key: "key2" });
    db.c1.save({ _key: "key3" });
  }
});
    db.c1.count(); // 3 
```

Aborting the transaction by throwing an exception in the *action* function
will revert all changes, so as if the transaction never happened:

```js
// setup
db._create("c1");

db._executeTransaction({
  collections: {
    write: [ "c1" ]
  },
  action: function () {
    var db = require("@arangodb").db;
    db.c1.save({ _key: "key1" });
    db.c1.count(); // 1 
    db.c1.save({ _key: "key2" });
    db.c1.count(); // 2 
    throw "doh!";
  }
});

db.c1.count(); // 0 
```

The automatic rollback is also executed when an internal exception is thrown 
at some point during transaction execution:

```js
// setup 
db._create("c1");

db._executeTransaction({
  collections: {
    write: [ "c1" ]
  },
  action: function () {
    var db = require("@arangodb").db;
    db.c1.save({ _key: "key1" });
    // will throw duplicate a key error, not explicitly requested by the user 
    db.c1.save({ _key: "key1" });  
    // we'll never get here... 
  }
});

db.c1.count(); // 0 
```

As required by the *consistency* principle, aborting or rolling back a 
transaction will also restore secondary indexes to the state at transaction
start.

### Cross-collection transactions

There's also the possibility to run a transaction across multiple collections. 
In this case, multiple collections need to be declared in the *collections*
attribute, e.g.:

```js
// setup 
db._create("c1");
db._create("c2");

db._executeTransaction({
  collections: {
    write: [ "c1", "c2" ]
  },
  action: function () {
    var db = require("@arangodb").db;
    db.c1.save({ _key: "key1" });
    db.c2.save({ _key: "key2" });
  }
});

db.c1.count(); // 1 
db.c2.count(); // 1
```

Again, throwing an exception from inside the *action* function will make the 
transaction abort and roll back all changes in all collections:

```js
// setup 
db._create("c1");
db._create("c2");

db._executeTransaction({
  collections: {
    write: [ "c1", "c2" ]
  },
  action: function () {
    var db = require("@arangodb").db;
    for (var i = 0; i < 100; ++i) {
      db.c1.save({ _key: "key" + i });
      db.c2.save({ _key: "key" + i });
    }
    db.c1.count(); // 100 
    db.c2.count(); // 100 
    // abort 
    throw "doh!"
  }
});

db.c1.count(); // 0 
db.c2.count(); // 0 
```
