Features and Improvements in ArangoDB 3.3
=========================================

The following list shows in detail which features have been added or improved in
ArangoDB 3.3. ArangoDB 3.3 also contains several bugfixes that are not listed
here.

Datacenter-to-datacenter replication (DC2DC)
--------------------------------------------

Every company needs a disaster recovery plan for all important systems.
This is true from small units like single processes running in some
container to the largest distributed architectures. For databases in
particular this usually involves a mixture of fault-tolerance,
redundancy, regular backups and emergency plans. The larger a
data store, the more difficult it is to come up with a good strategy.

Therefore, it is desirable to be able to run a distributed database
in one data-center and replicate all transactions to another
data-center in some way. Often, transaction logs are shipped
over the network to replicate everything in another, identical
system in the other data-center. Some distributed data stores have
built-in support for multiple data-center awareness and can
replicate between data-centers in a fully automatic fashion.

ArangoDB 3.3 takes an evolutionary step forward by introducing
multi-data-center support, which is asynchronous data-center to
data-center replication. Our solution is asynchronous and scales
to arbitrary cluster sizes, provided your network link between
the data-centers has enough bandwidth. It is fault-tolerant
without a single point of failure and includes a lot of
metrics for monitoring in a production scenario.

[DC2DC](../Deployment/DC2DC/README.md) is available in the *Enterprise Edition*.

Encrypted backups
-----------------

Arangodump can now create encrypted backups using AES256 for encryption.
The encryption key can be read from a file or from a generator program.
It works in single server and cluster mode.

Example for non-encrypted backup (everyone with access to the backup will be
able to read it):

    arangodump --collection "secret" dump 

In order to create an encrypted backup, add the `--encryption.keyfile`
option when invoking arangodump:

    arangodump --collection "secret" dump --encryption.keyfile ~/SECRET-KEY

The key must be exactly 32 bytes long (required by the AES block cipher).

Note that arangodump will not store the key anywhere. It is the responsibility
of the user to find a safe place for the key. However, arangodump will store
the used encryption method in a file named `ENCRYPTION` in the dump directory.
That way arangorestore can later find out whether it is dealing with an
encrypted dump or not.

Trying to restore the encrypted dump without specifying the key will fail:

    arangorestore --collection "secret-collection" dump --create-collection true

arangorestore will complain with:

```
the dump data seems to be encrypted with aes-256-ctr, but no key information was specified to decrypt the dump
it is recommended to specify either `--encryption.keyfile` or `--encryption.key-generator` when invoking arangorestore with an encrypted dump
```

It is required to use the exact same key when restoring the data. Again this is
done by providing the `--encryption.keyfile` parameter:

    arangorestore --collection "secret-collection" dump --create-collection true --encryption.keyfile ~/SECRET-KEY

Using a different key will lead to the backup being non-recoverable.

Note that encrypted backups can be used together with the already existing 
RocksDB encryption-at-rest feature, but they can also be used for the MMFiles
engine, which does not have encryption-at-rest.

[Encrypted backups](../Programs/Arangodump/Examples.md#encryption) are available
in the *Enterprise Edition*.

Server-level replication
------------------------

ArangoDB supports asynchronous replication functionality since version 1.4, but
replicating from a master server with multiple databases required manual setup
on the slave for each individual database to replicate. When a new database was
created on the master, one needed to take action on the slave to ensure that data
for that database got actually replicated. Replication on the slave also was not
aware of when a database was dropped on the master.

3.3 adds [server-level replication](../Administration/MasterSlave/ServerLevelSetup.md),
which will replicate the current and future databases from the master to the
slave automatically after the initial setup.

In order to set up global replication on a 3.3 slave for all databases of a given 
3.3 master, there is now the so-called `globalApplier`. It has the same interface
as the existing `applier`, but it will replicate from all databases of the
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
```

To check if the applier is running, also use the `globalApplier` object:

```js
replication.globalApplier.state().state
```

The server-level replication requires both the master and slave servers to 
ArangoDB version 3.3 or higher.

Asynchronous failover
---------------------

A resilient setup can now easily be achieved by running a pair of connected servers, 
of which one instance becomes the master and the other an asynchronously replicating 
slave, with automatic failover between them.

Two servers are connected via asynchronous replication. One of the servers is
elected leader, and the other one is made a follower automatically. At startup,
the two servers fight for leadership. The follower will automatically start
replication from the master for all available databases, using the server-level
replication introduced in 3.3.

When the master goes down, this is automatically detected by an agency
instance, which is also started in this mode. This instance will make the
previous follower stop its replication and make it the new leader.

The follower will automatically deny all read and write requests from client
applications. Only the replication itself is allowed to access the follower's data
until the follower becomes a new leader.

When sending a request to read or write data on a follower, the follower will 
always respond with `HTTP 503 (Service unavailable)` and provide the address of
the current leader. Client applications and drivers can use this information to 
then make a follow-up request to the proper leader:

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
ArangoDB support automatic failover in case the currently accessed server endpoint 
responds with HTTP 503.

Blog article:
[Introducing the new ArangoDB Java driver with load balancing and advanced fallback](https://www.arangodb.com/2017/12/introducing-the-new-arangodb-java-driver-load-balancing/)

RocksDB throttling
------------------

ArangoDB 3.3 allows write operations to the RocksDB engine be throttled, in
order to prevent longer write stalls. The throttling is adaptive, meaning that it
automatically adapts to the actual write rate. This results in much more stable
response times, which is better for client applications and cluster health
tests, because timeouts caused by write stalls are less likely to occur and
the server thus not mistakenly assumed to be down.

Blog article:
[RocksDB smoothing for ArangoDB customers](https://www.arangodb.com/2017/11/rocksdb-smoothing-arangodb-customers/)

Faster shard creation in cluster
--------------------------------

When using a cluster, one normally wants resilience, so `replicationFactor`
is set to at least `2`. The number of shards is often set to rather high values
when creating collections. 

Creating a collection in the cluster will make the coordinator store the setup
metadata of the new collection in the agency first. Subsequentially all database
servers of the cluster will detect that there is work to do and will begin creating
the shards. This will first happen for the shard leaders. For each shard leader
that finishes with the setup, the synchronous replication with its followers is
then established. That will make sure that every future data modification will not 
become effective on the leader only, but also on all the followers.

In 3.3 this setup protocol has got some shortcuts for the initial shard creation, 
which speeds up collection creation by roughly 50 to 60 percent.

LDAP authentication
-------------------

The LDAP authentication module in the *Enterprise Edition* has been enhanced.
The following options have been added to it:
 
- the option `--server.local-authentication` controls whether the local *_users*
  collection is also used for looking up users. This is also the default behavior.
  If the authentication shall be restricted to just the LDAP directory, the
  option can be set to *true*, and arangod will then not make any queries to its
  *_users* collection when looking up users.

- the option `--server.authentication-timeout` controls the expiration time for 
  cached LDAP user information entries in arangod.

- basic role support has been added for the LDAP module in the *Enterprise Edition*.
  New configuration options for LDAP in 3.3 are:

  - `--ldap.roles-attribute-name`
  - `--ldap.roles-transformation`
  - `--ldap.roles-search`
  - `--ldap.roles-include`
  - `--ldap.roles-exclude`
  - `--ldap.superuser-role`

  Please refer to [LDAP](../Programs/Arangod/Ldap.md) for a detailed
  explanation.


Miscellaneous features
----------------------

- when creating a collection in the cluster, there is now an optional 
  parameter `enforceReplicationFactor`: when set, this parameter
  enforces that the collection will only be created if there are not
  enough database servers available for the desired `replicationFactor`.

- AQL DISTINCT is not changing the order of previous (sorted) results

  Previously the implementation of AQL distinct stored all encountered values
  in a hash table internally. When done, the final results were returned in the
  order dictated by the hash table that was used to store the keys. This order
  was more or less unpredictable. Though this was documented behavior, it was
  inconvenient for end users.

  3.3 now does not change the sort order of the result anymore when DISTINCT 
  is used.

- Several AQL functions have been implemented in C++, which can help save
  memory and CPU time for converting the function arguments and results.
  The following functions have been ported:

  - LEFT
  - RIGHT
  - SUBSTRING
  - TRIM
  - MATCHES

- The ArangoShell prompt substitution characters have been extended. Now the
  following extra substitutions can be used for the arangosh prompt:
  
  - '%t': current time as timestamp
  - '%a': elpased time since ArangoShell start in seconds
  - '%p': duration of last command in seconds
 
  For example, to show the execution time of the last command executed in arangosh
  in the shell's prompt, start arangosh using:

      arangosh --console.prompt "%E@%d %p> "

- There are new startup options for the logging to aid debugging and error reporting:

  - `--log.role`: will show one-letter code of server role (A = agent, C = coordinator, ...)
    This is especially useful when aggregating logs.

    The existing roles used in logs are:

    - U: undefined/unclear (used at startup)
    - S: single server
    - C: coordinator
    - P: primary
    - A: agent

  - `--log.line-number true`: this option will now additionally show the name of the C++ 
    function that triggered the log message (file name and line number were already logged 
    in previous versions)
    
  - `--log.thread-name true`: this new option will log the name of the ArangoDB thread that 
    triggered the log message. Will have meaningful output on Linux only

- make the ArangoShell (arangosh) refill its collection cache when a yet-unknown collection
  is first accessed. This fixes the following problem when working with the shell while
  in another shell or by another process a new collection is added:

      arangosh1> db._collections();  // shell1 lists all collections
      arangosh2> db._create("test"); // shell2 now creates a new collection 'test'
      arangosh1> db.test.insert({}); // shell1 is not aware of the collection created
                                     // in shell2, so the insert will fail

