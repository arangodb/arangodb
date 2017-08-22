Known Issues
============

The following known issues are present in this version of ArangoDB and will be fixed
in follow-up releases:

### RocksDB storage engine

The RocksDB storage engine is intentionally missing the following features that 
are present in the MMFiles engine:

* the datafile debugger (arango-dfdb) cannot be used with this storage engine

  RocksDB has its own crash recovery so using the dfdb will not make any sense here.

* APIs that return collection properties or figures will return slightly different
  attributes for the RocksDB engine than for the MMFiles engine. For example, the
  attributes `journalSize`, `doCompact`, `indexBuckets` and `isVolatile` are present
  in the MMFiles engine but not in the RocksDB engine. The memory usage figures reported 
  for collections in the RocksDB engine are estimate values, whereas they are
  exact for the MMFiles engine.

* the RocksDB engine does not support some operations which only make sense in the
  context of the MMFiles engine. These are:

  - the `rotate` method on collections
  - the `flush` method for WAL files

* the RocksDB storage engine does not support volatile collections

* transactions are limited in size. Transactions that get too big (in terms of
  number of operations involved or the total size of data modified by the transaction)
  will be committed automatically. Effectively this means that big user transactions
  are split into multiple smaller RocksDB transactions that are committed individually.
  The entire user transaction will not necessarily have ACID properties in this case.

  The threshold values for transaction sizes can be configured globally using the
  startup options

  * `--rocksdb.intermediate-commit-size`: if the size of all operations in a transaction 
    reaches this threshold, the transaction is committed automatically and a new transaction
    is started. The value is specified in bytes.

  * `--rocksdb.intermediate-commit-count`: if the number of operations in a transaction 
    reaches this value, the transaction is committed automatically and a new transaction
    is started.

  * `--rocksdb.max-transaction-size`: this is an upper limit for the total number of bytes
    of all operations in a transaction. If the operations in a transaction consume more
    than this threshold value, the transaction will automatically abort with error 32
    ("resource limit exceeded").

  It is also possible to override these thresholds per transaction.

The following known issues will be resolved in future releases:

* the RocksDB engine is not yet performance-optimized and potentially not well configured

* collections for which a geo index is present will use collection-level write locks 
  even with the RocksDB engine. Reads from these collections can still be done in parallel 
  but no writes

* modifying documents in a collection with a geo index will cause multiple additional 
  writes to RocksDB for maintaining the index structures

* the number of documents reported for collections (`db.<collection>.count()`) may be
  slightly wrong during transactions if there are parallel transactions ongoing for the
  same collection that also modify the number of documents

* the `any` operation to provide a random document from a collection is supported
  by the RocksDB engine but the operation has much higher algorithmic complexity than 
  in the MMFiles engine. It is therefore discouraged to call it for cases other than manual
  inspection of a few documents in a collection

* AQL queries in the cluster still issue an extra locking HTTP request per shard though
  this would not be necessary for the RocksDB engine in most cases

### Installer

* Upgrading from 3.1 to 3.2 on Windows requires the user to manually copy the database directory
  to the new location and run an upgrade on the database. Please consult the
  [Documentation](../GettingStarted/Installing/Windows.md)
  for detailed instructions.

### System Integration

* On some Linux systems systemd and system v might report that the arangodb
  service is in good condition when it could not be started. In this case the
  user needs to check `/var/log/arangodb3` for further information about the
  failed startup.

### Mac OS X

 * Storage engine is not changeable on an existing database. Currently only the initial selection of the storage engine is supported.
   In order to use another storage engine, you have to delete your ArangoDB application (Mac Application Folder)
   and `/Users/<your_user_name>/Library/ArangoDB` folder.

### OpenSSL 1.1

 * ArangoDB has been tested with OpenSSL 1.0 only and won't build against 1.1 when compiling on your own. See [here](../../cookbook/Compiling/OpenSSL.html)
   for how to compile on systems that ship OpenSSL 1.1 by default.
