# Storage Engines

At the very bottom of the ArangoDB database lies the storage
engine. The storage engine is responsible for persisting the documents
on disk, holding copies in memory, providing indexes and caches to
speed up queries.

Up to version 3.1 ArangoDB only supported memory mapped files (MMFILES)
as sole storage engine. Beginning with 3.2 ArangoDB has support for
pluggable storage engines. The second supported engine is RocksDB from
Facebook.

RocksDB is an embeddable persistent key-value store. It is a log
structure database and is optimized for fast storage.

The MMFILES engine is optimized for the use-case where the data fits
into the main memory. It allows for very fast concurrent
reads. However, writes block reads and locking is on collection
level. Indexes are always in memory and are rebuilt on startup. This
gives better performance but imposes a longer startup time.

The ROCKSDB engine is optimized for large data-sets and allows for a
steady insert performance even if the data-set is much larger than the
main memory. Indexes are always stored on disk but caches are used to
speed up performance. RocksDB uses document-level locks allowing for
concurrent writes. Writes do not block reads. Reads do not block writes.

The engine must be selected for the whole server / cluster. It is not
possible to mix engines. The transaction handling and write-ahead-log
format in the individual engines is very different and therefore cannot 
be mixed.

## RocksDB

### Advantages

RocksDB is a very flexible engine that can be configured for various use cases.

The main advantages of RocksDB are

- document-level locks
- support for large data-sets
- persistent indexes

### Caveats

RocksDB allows concurrent writes. However, when touching the same document a
write conflict is raised. This cannot happen with the MMFILES engine, therefore
applications that switch to RocksDB need to be prepared that such exception can
arise. It is possible to exclusively lock collections when executing AQL. This
will avoid write conflicts but also inhibits concurrent writes.

Currently, another restriction is due to the transaction handling in
RocksDB. Transactions are limited in total size. If you have a statement
modifying a lot of documents it is necessary to commit data inbetween. This will
be done automatically for AQL by default.

### Performance

RocksDB is a based on log-structured merge tree. A good introduction can be
found in:

- http://www.benstopford.com/2015/02/14/log-structured-merge-trees/
- https://blog.acolyer.org/2014/11/26/the-log-structured-merge-tree-lsm-tree/

The basic idea is that data is organized in levels were each level is a factor
larger than the previous. New data will reside in smaller levels while old data
is moved down to the larger levels. This allows to support high rate of inserts
over an extended period. In principle it is possible that the different levels
reside on different storage media. The smaller ones on fast SSD, the larger ones
on bigger spinning disks.

RocksDB itself provides a lot of different knobs to fine tune the storage
engine according to your use-case. ArangoDB supports the most common ones
using the options below.

Performance reports for the storage engine can be found here:

- https://github.com/facebook/rocksdb/wiki/performance-benchmarks
- https://github.com/facebook/rocksdb/wiki/RocksDB-Tuning-Guide

### ArangoDB options

ArangoDB has a cache for the persistent indexes in RocksDB. The total size 
of this cache is controlled by the option

    --cache.size

RocksDB also has a cache for the blocks stored on disk. The size of
this cache is controlled by the option

    --rocksdb.block-cache-size

ArangoDB distributes the available memory equally between the two
caches by default.

ArangoDB chooses a size for the various levels in RocksDB that is
suitable for general purpose applications.

RocksDB log strutured data levels have increasing size

    MEM: --
    L0:  --
    L1:  -- --
    L2:  -- -- -- --
    ...

New or updated Documents are first stored in memory. If this memtable
reaches the limit given by

    --rocksdb.write-buffer-size 

it will converted to an SST file and inserted at level 0.

The following option controls the size of each level and the depth.

    --rocksdb.num-levels N

Limits the number of levels to N. By default it is 7 and there is
seldom a reason to change this. A new level is only opened if there is
too much data in the previous one.

    --rocksdb.max-bytes-for-level-base B

L0 will hold at most B bytes.

    --rocksdb.max-bytes-for-level-multiplier M

Each level is at most M times as much bytes as the previous
one. Therefore the maximum number of bytes forlevel L can be
calculated as

    max-bytes-for-level-base * (max-bytes-for-level-multiplier ^ (L-1))

## Future

RocksDB imposes a limit on the transaction size. It is optimized to
handle small transactions very efficiently, but is effectively limiting 
the total size of transactions.

ArangoDB currently uses RocksDB's transactions to implement the ArangoDB 
transaction handling. Therefore the same restrictions apply for ArangoDB
transactions when using the RocksDB engine.

We will improve this by introducing distributed transactions in a future
version of ArangoDB. This will allow handling large transactions as a 
series of small RocksDB transactions and hence removing the size restriction.
