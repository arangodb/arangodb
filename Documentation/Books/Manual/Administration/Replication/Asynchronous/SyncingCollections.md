Syncing Collections
===================

In order to synchronize data for a single collection from a master to a slave instance, there
is the *syncCollection* function:

It will fetch all documents of the specified collection from the master database and store 
them in the local instance. After the synchronization, the collection data on the slave will be
identical to the data on the master, provided no further data changes happen on the master. 
Any data changes that are performed on the master after the synchronization was started will
not be captured by *syncCollection*, but need to be replicated using the regular replication
applier mechanism.
  
For the following example setup, we'll use the instance *tcp://master.domain.org:8529* as the 
master, and the instance *tcp://slave.domain.org:8530* as a slave.

The goal is to have all data from the collection *test* in database *_system* on master 
*tcp://master.domain.org:8529* be replicated to the collection *test* in database *_system* on 
the slave *tcp://slave.domain.org:8530*.

On the **master**, the collection *test* needs to be present in the *_system* database, with
any data in it.

To transfer this collection to the **slave**, issue the following commands there:

```js
db._useDatabase("_system");
require("@arangodb/replication").syncCollection("test", {
  endpoint: "tcp://master.domain.org:8529",
  username: "myuser",
  password: "mypasswd"
});
```

**Warning**: The syncCollection command will replace the collection's data in the slave database 
with data from the master database! Only execute these commands if you have verified you are on 
the correct server, in the correct database!

Setting the optional *incremental* attribute in the call to *syncCollection* will start an
incremental transfer of data. This may be useful in case when the slave already has
parts or almost all of the data in the collection and only the differences need to be
synchronized. Note that to compute the differences the incremental transfer will build a sorted
list of all document keys in the collection on both the slave and the master, which may still be
expensive for huge collections in terms of memory usage and runtime. During building the list
of keys the collection will be read-locked on the master.

The *initialSyncMaxWaitTime* attribute in the call to *syncCollection* controls how long the
slave will wait for a master's response. This wait time can be used to control after what time 
the synchronization will give up and fail. 

The *syncCollection* command may take a long time to complete if the collection is big. The shell
will block until the slave has synchronized the entire collection from the master or until an 
error occurs. By default, the *syncCollection* command in the ArangoShell will poll for a status
update every 10 seconds.

When *syncCollection* is called from the ArangoShell, the optional *async* attribute can be used
to start the synchronization as a background process on the slave. If *async* is set to *true*,
the call to *syncCollection* will return almost instantly with an id string. Using this id string,
the status of the sync job on the slave can be queried using the *getSyncResult* function as follows:

```js
db._useDatabase("_system");
var replication = require("@arangodb/replication");

/* run command in async mode */
var id = replication.syncCollection("test", {
  endpoint: "tcp://master.domain.org:8529",
  username: "myuser",
  password: "mypasswd",
  async: true
});

/* now query the status of our operation */
print(replication.getSyncResult(id));
```

*getSyncResult* will return *false* as long as the synchronization is not complete, and return the
synchronization result otherwise.
