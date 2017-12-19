Features and Improvements
=========================

The following list shows in detail which features have been added or improved in
ArangoDB 3.3. ArangoDB 3.3 also contains several bugfixes that are not listed
here.


Datacenter-to-datacenter replication (DC2DC)
--------------------------------------------



[DC2DC](#) is available in the *Enterprise* edition.

Encrypted backups
-----------------

Arangodump can create encrypted backups using AES256 for encryption.
The encryption key can be read from a file or from a generator program.
It works in single server and cluster mode.

Example for non-encrypted backup (everyone with access to the backup will be
able to read it):

    arangodump --collection "secret" dump 

In order to create an encrypted backup, simply add the `--encryption.keyfile`
option when invoking arangodump:

    arangodump --collection "secret" dump --encryption.keyfile ~/SECRET-KEY

The key must be exactly 32 bytes long (required by the AES block cipher).

Note that arangodump will not store the key anywhere. It is the responsibility
of the user to find a save place for the key. However, arangodump will store
the used encryption method in a file named `ENCRYPTION` in the dump directory.
That way, arangorestore can later find out whether it is dealing with an
encrypted dump or not.

Trying to restore the encrypted dump without specifying the key will fail:

    arangorestore --collection "secret-collection" dump --create-collection true

arangorestore will complain with:

> the dump data seems to be encrypted with aes-256-ctr, but no key information
> was specified to decrypt the dump it is recommended to specify either
> `--encryption.key-file` or `--encryption.key-generator` when invoking
> arangorestore with an encrypted dump

It is required to use the exact same key when restoring the data. Again this is
done by providing the `--encryption.keyfile` parameter:

    arangorestore --collection "secret-collection" dump --create-collection true --encryption.keyfile ~/SECRET-KEY

Using a different key will lead to the backup being non-recoverable.

Note that encrypted backups can be used together with the already existing 
RocksDB encryption-at-rest feature, but they can also be used for the MMFiles
engine, which does not have encryption-at-rest.

[Encrypted backups](#) is available in the *Enterprise* edition.

Server-global replication
-------------------------

ArangoDB supports asynchronous replication functionality since version 1.4, but
replicating from a master server with multiple databases required manual fiddling
on the slave for each individual database to replicate. When a new database was
created on the master, one needed to take action on the slave to ensure that data
for that database got actually replicated. Replication on the slave also was not
aware of when a database was dropped on the master.

3.3 adds server-global replication, which replicates all current and future
databases from the master to the slave automatically after the initial start.

In order to set up replication on a 3.3 slave for all databases of a given 3.3
master, there is now the so-called `globalApplier`. It has the same interface
as the existing `applier`, but it will replicate from all databases on the
master and not just a single one.

In order to start the replication on the slave and make it replicate all
databases from a given master, use these commands on the slave:

```js
var replication = require("@arangodb/replication");

replication.setupReplicationGlobal({
  endpoint: "tcp://127.0.0.1:8529",
  username: "root",
  password: "",
  autoStart: true
});

// to check if the applier is running, now use the `globalApplier` object
replication.globalApplier.state().state
```

The server-global replication requires master and slave both to be at least
ArangoDB version 3.3.

Asynchronous failover
---------------------

A single resilient server can now easily be achieved by running a pair of
connected servers, of which one instance becomes the master and the other
an asynchronously replicating slave, with automatic failover between them.

Two servers are connected via asynchronous replication. One of the servers is
elected leader, and the other one is made a follower automatically. At startup,
the two servers fight for leadership. The follower will automatically start
replication from the master for all databases, using the new server-global
replication.

When the master goes down, this is automatically detected by an agency
instance, which is also started in this mode. This instance will make the
previous follower stop its replication and make it the new leader.

The follower will automatically deny all read and write requests from client
applications. Only the replication is allowed to access the follower's data
until the follower becomes a new leader.

When sending a request to read or write data on a follower, it will always
respond with `HTTP 503 (Service unavailable)` and the address of the current
leader. Client applications and drivers can use this information to then make
a follow-up request to the proper leader:

```
HTTP/1.1 503 Service Unavailable
X-Arango-Endpoint: http://[::1]:8531
....
```

Client applications can also detect who the current leader and the followers
are by calling the `/_api/cluster/endpoints` REST API. This API is accessible
on leaders and followers alike.

The ArangoDB starter supports starting two servers with asynchronous
replication and failover out of the box.

The arangojs driver for JavaScript, the Go driver and the Java driver for
ArangoDB already (or will soon) support automatic failover in case the
currently used server endpoint responds with HTTP 503.

Blog article:
[Introducing the new ArangoDB Java driver with load balancing and advanced fallback
](https://www.arangodb.com/2017/12/introducing-the-new-arangodb-java-driver-load-balancing/)

RocksDB throttling
------------------

Write operations to RocksDB with the RocksDB storage engine are throttled, in
order to prevent total stalls. The throttling is adaptive, meaning that it
automatically adapts to the actual write rate. This results in much more stable
response times, which is better for client applications and cluster health
tests, because timeouts caused by write stalls are less likely to occur and
the server thus not mistakenly assumed to be down.

Blog article: [RocksDB smoothing for ArangoDB customers
](https://www.arangodb.com/2017/11/rocksdb-smoothing-arangodb-customers/)

Faster shard creation in cluster
--------------------------------

When using a cluster, one normally wants resilience, so `replicationFactor`
is set to at least `2`. The number of shards is often set to pretty high values.
Some people create collections with 100 shards even.

Internally, this will first store the collection metadata in the agency, and
then the assigned shard leaders will pick up the change and will begin creating
the shards. When the shards are set up on the leader, the replication is
kicked off so every data modification will not become effective on the leader
only, but also on the followers.

This process has got some shortcuts for the initial creation of shards in 3.3.

Running the following script against a (local) cluster shows the required time
to drop by about 67% when comparing 3.3 to 3.2:

```js
var time = require("internal").time; 
s = time(); 
for (i = 0; i < 15; ++i) { 
  db._create("test" + i, { numberOfShards: 4, replicationFactor: 2 }); 
  print(i); 
} 
time() - s;
```

LDAP roles and aliases
----------------------


Read-only mode for servers
--------------------------


Miscellaneous features
----------------------

- AQL DISTINCT not changing the order of previous (sorted) results:

  Assume there is a query with a defined result set sort oder, but there is the
  demand to make the results distinct later, e.g.:

  ```js
  LET docs = (
    FOR doc IN test 
      SORT doc.value 
      RETURN doc
  ) 

  // further processing done here

  FOR doc IN docs 
    RETURN DISTINCT doc.value
  ```

  In 3.2, DISTINCT changed the order of the previously sorted result, witout a
  way to avoid this. The behavior is changed in 3.3 to retain the order.

- Added C++ implementations for some AQL functions. This can speed up the
  following AQL functions and makes them use less memory:

  - LEFT
  - RIGHT
  - SUBSTRING
  - TRIM
  - MATCHES

- Show the execution time of last command executed in ArangoShell in the shell
  prompt:

      arangosh --console.prompt "%E@%d %p> "

- More context for debug logging

  There are new logging options to diagnose problems, mainly for ArangoDB
  developers and support:

  - `--log.role`: will show one-letter code of server role (A = agent, C = coordinator, ...)
    this is useful when aggregating logs

  - `--log.line-number true`: will now additionally show name of the C++ function that 
    triggered the log message (file name and line number were already logged before)
    
  - `--log.thread-name true`: will log the name of the ArangoDB thread that triggered the
    log message
