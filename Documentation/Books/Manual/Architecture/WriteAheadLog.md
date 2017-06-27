Write-ahead log
===============
The Write-ahead log is part of the MMFiles storage engine; This doesn't apply to your
ArangoDB if you are running with the [RocksDB](../Administration/Configuration/RocksDB.md)
storage engine.

Starting with version 2.2 ArangoDB stores all data-modification operation in
its write-ahead log. The write-ahead log is sequence of append-only files containing
all the write operations that were executed on the server.

It is used to run data recovery after a server crash, and can also be used in
a replication setup when slaves need to replay the same sequence of operations as
on the master.

By default, each write-ahead logfile is 32 MiB in size. This size is configurable via the
option *--wal.logfile-size*.

When a write-ahead logfile is full, it is set to read-only, and following operations will
be written into the next write-ahead logfile. By default, ArangoDB will reserve some
spare logfiles in the background so switching logfiles should be fast. How many reserve
logfiles ArangoDB will try to keep available in the background can be controlled by the
configuration option *--wal.reserve-logfiles*.

Data contained in full write-ahead files will eventually be transferred into the journals or
datafiles of collections. Only the "surviving" documents will be copied over. When all
remaining operations from a write-ahead logfile have been copied over into the journals
or datafiles of the collections, the write-ahead logfile can safely be removed if it is
not used for replication.

Long-running transactions prevent write-ahead logfiles from being fully garbage-collected
because it is unclear whether a transaction will commit or abort. Long-running transactions
can thus block the garbage-collection progress and should therefore be avoided at 
all costs.

On a system that acts as a replication master, it is useful to keep a few of the 
already collected write-ahead logfiles so replication slaves still can fetch data from
them if required. How many collected logfiles will be kept before they get deleted is
configurable via the option *--wal.historic-logfiles*.

For all write-ahead log configuration options, please refer to the page [Write-ahead log options](../Administration/Configuration/Wal.md).
