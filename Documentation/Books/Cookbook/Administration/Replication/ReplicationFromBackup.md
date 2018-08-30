# Speeding up slave initialization

## Problem

You have a very big database and want to set up a `master-slave` replication between two or more ArangoDB instances. Transfering the entire database over the network may take a long time, if the database is big. In order to speed-up the replication initialization process the **slave** can be initialized using a backup of the **master**.

For the following example setup, we will use the instance with endpoint `tcp://master.domain.org:8529` as master, and the instance with endpoint `tcp://slave.domain.org:8530` as slave.

The goal is to have all data from the database ` _system` on master replicated to the database `_system` on the slave (the same process can be applied for other databases) .

## Solution
 
First of all you have to start the master server, using a command like the above:

```sh
arangod --server.endpoint tcp://master.domain.org:8529
```

Depending on your storage engine you also want to adjust the following options:

For MMFiles:

```sh
--wal.historic-logfiles		(maximum number of historic logfiles to keep after collection
                                (default: 10))
```

For RocksDB:

```sh
--rocksdb.wal-file-timeout	(timeout after which unused WAL files are deleted
                                in seconds (default: 10))
```

The options above prevent the premature removal of old WAL files from the master, and are useful in case intense write operations happen on the master while you are initializing the slave. In fact, if you do not tune these options, what can happen is that the master WAL files do not include all the write operations happened after the backup is taken. This may lead to situations in which the initialized slave is missing some data, or fails to start.

Now you have to create a dump from the master using the tool `arangodump`:

```sh
arangodump --output-directory "dump" --server.endpoint tcp://master.domain.org:8529
```

Please adapt the `arangodump` command to your specific case.

The following is a possible `arangodump` output:

```sh
Server version: 3.3
Connected to ArangoDB 'tcp://master.domain.org:8529', database: '_system', username: 'root'
Writing dump to output directory 'dump'
Last tick provided by server is: 37276350
# Dumping document collection 'TestNums'...
# Dumping document collection 'TestNums2'...
# Dumping document collection 'frenchCity'...
# Dumping document collection 'germanCity'...
# Dumping document collection 'persons'...
# Dumping edge collection 'frenchHighway'...
# Dumping edge collection 'germanHighway'...
# Dumping edge collection 'internationalHighway'...
# Dumping edge collection 'knows'...
Processed 9 collection(s), wrote 1298855504 byte(s) into datafiles, sent 32 batch(es)
```

In line *4* the last server `tick` is displayed. This value will be useful when we will start the replication, to have the `replication-applier` start replicating exactly from that `tick`. 

Next you have to start the slave:

```sh
arangod --server.endpoint tcp://slave.domain.org:8530
```

If you are running master and slave on the same server (just for test), please make sure you give your slave a different data directory.

Now you are ready to restore the dump with the tool `arangorestore`:

```sh
arangorestore --input-directory "dump" --server.endpoint tcp://slave.domain.org:8530
```

Again, please adapt the command above in case you are using a database different than `_system`.

Once the restore is finished there are two possible approaches to start the replication.

### Approach 1: All-in-one setup

Start replication on the slave with `arangosh` using the following command:

```sh
arangosh --server.endpoint tcp://slave.domain.org:8530
```

```js
db._useDatabase("_system");
require("@arangodb/replication").setupReplication({
  endpoint: "tcp://master.domain.org:8529",
  username: "myuser",
  password: "mypasswd",
  verbose: false,
  includeSystem: false,
  incremental: true,
  autoResync: true
});
```

The following is the printed output:

```sh
still synchronizing... last received status: 2017-12-06T14:06:25Z: fetching collection keys for collection 'TestNums' from /_api/replication/keys/keys?collection=7173693&to=57482456&serverId=24282855553110&batchId=57482462
still synchronizing... last received status: 2017-12-06T14:06:25Z: fetching collection keys for collection 'TestNums' from /_api/replication/keys/keys?collection=7173693&to=57482456&serverId=24282855553110&batchId=57482462
[...]
still synchronizing... last received status: 2017-12-06T14:07:13Z: sorting 10000000 local key(s) for collection 'TestNums'
still synchronizing... last received status: 2017-12-06T14:07:13Z: sorting 10000000 local key(s) for collection 'TestNums'
[...]
still synchronizing... last received status: 2017-12-06T14:09:10Z: fetching master collection dump for collection 'TestNums3', type: document, id 37276943, batch 2, markers processed: 15278, bytes received: 2097258
still synchronizing... last received status: 2017-12-06T14:09:18Z: fetching master collection dump for collection 'TestNums5', type: document, id 37276973, batch 5, markers processed: 123387, bytes received: 17039688
[...]
still synchronizing... last received status: 2017-12-06T14:13:49Z: fetching master collection dump for collection 'TestNums5', type: document, id 37276973, batch 132, markers processed: 9641823, bytes received: 1348744116
still synchronizing... last received status: 2017-12-06T14:13:59Z: fetching collection keys for collection 'frenchCity' from /_api/replication/keys/keys?collection=27174045&to=57482456&serverId=24282855553110&batchId=57482462
{ 
  "state" : { 
    "running" : true, 
    "lastAppliedContinuousTick" : null, 
    "lastProcessedContinuousTick" : null, 
    "lastAvailableContinuousTick" : null, 
    "safeResumeTick" : null, 
    "progress" : { 
      "time" : "2017-12-06T14:13:59Z", 
      "message" : "send batch finish command to url /_api/replication/batch/57482462?serverId=24282855553110", 
      "failedConnects" : 0 
    }, 
    "totalRequests" : 0, 
    "totalFailedConnects" : 0, 
    "totalEvents" : 0, 
    "totalOperationsExcluded" : 0, 
    "lastError" : { 
      "errorNum" : 0 
    }, 
    "time" : "2017-12-06T14:13:59Z" 
  }, 
  "server" : { 
    "version" : "3.3.devel", 
    "serverId" : "24282855553110" 
  }, 
  "endpoint" : "tcp://master.domain.org:8529", 
  "database" : "_system" 
}
```

This is the same command that you would use to start replication even without taking a backup first. The difference, in this case, is that the data that is present already on the slave (and that has been restored from the backup) this time is not transferred over the network from the master to the slave. 

The command above will only check that the data already included in the slave is in sync with the master. After this check, the `replication-applier` will make sure that all write operations that happened on the master after the backup are replicated on the slave.

While this approach is definitely faster than transferring the whole database over the network, since a sync check is performed, it can still require some time. 

### Approach 2: Apply replication by tick

In this approach, the sync check described above is not performed. As a result this approach is faster as the existing slave data is not checked. Write operations are executed starting from the `tick` you provide and continue with the master's available `ticks`.  

This is still a secure way to start replication as far as the correct `tick` is passed.

As previously mentioned the last `tick` provided by the master is displayed when using `arangodump`. In our example the last tick was **37276350**.

First of all you have to apply the properties of the replication, using `arangosh` on the slave:

```sh
arangosh --server.endpoint tcp://slave.domain.org:8530
```

```js
db._useDatabase("_system");
require("@arangodb/replication").applier.properties({
  endpoint: "tcp://master.domain.org:8529",
  username: "myuser",
  password: "mypasswd",
  verbose: false,
  includeSystem: false,
  incremental: true,
  autoResync: true});
```

Then you can start the replication with the last provided `logtick` of the master (output of `arangodump`):

```js
require("@arangodb/replication").applier.start(37276350)
```

The following is the printed output:

```sh
{ 
  "state" : { 
    "running" : true, 
    "lastAppliedContinuousTick" : null, 
    "lastProcessedContinuousTick" : null, 
    "lastAvailableContinuousTick" : null, 
    "safeResumeTick" : null, 
    "progress" : { 
      "time" : "2017-12-06T13:26:04Z", 
      "message" : "applier initially created for database '_system'", 
      "failedConnects" : 0 
    }, 
    "totalRequests" : 0, 
    "totalFailedConnects" : 0, 
    "totalEvents" : 0, 
    "totalOperationsExcluded" : 0, 
    "lastError" : { 
      "errorNum" : 0 
    }, 
    "time" : "2017-12-06T13:33:25Z" 
  }, 
  "server" : { 
    "version" : "3.3.devel", 
    "serverId" : "176090204017635" 
  }, 
  "endpoint" : "tcp://master.domain.org:8529", 
  "database" : "_system" 
}
```

After the replication has been started with the command above, you can use the `applier.state` command to check how far the last applied `tick` on the slave is far from the last available master `tick`:

```sh
require("@arangodb/replication").applier.state()
{ 
  "state" : { 
    "running" : true, 
    "lastAppliedContinuousTick" : "42685113", 
    "lastProcessedContinuousTick" : "42685113", 
    "lastAvailableContinuousTick" : "57279944", 
    "safeResumeTick" : "37276974", 
    "progress" : { 
      "time" : "2017-12-06T13:35:25Z", 
      "message" : "fetching master log from tick 42685113, first regular tick 37276350, barrier: 0, open transactions: 1", 
      "failedConnects" : 0 
    }, 
    "totalRequests" : 190, 
    "totalFailedConnects" : 0, 
    "totalEvents" : 2704032, 
    "totalOperationsExcluded" : 0, 
    "lastError" : { 
      "errorNum" : 0 
    }, 
    "time" : "2017-12-06T13:35:25Z" 
  }, 
  "server" : { 
    "version" : "3.3.devel", 
    "serverId" : "176090204017635" 
  }, 
  "endpoint" : "tcp://master.domain.org:8529", 
  "database" : "_system" 
}
```


**Author:** [Max Kernbach](https://github.com/maxkernbach)

**Tags:** #database #replication #arangodump #arangorestore
