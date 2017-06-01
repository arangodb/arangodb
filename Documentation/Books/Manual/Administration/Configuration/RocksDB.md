RocksDB engine options
======================

RocksDB is a highly configurable key-value store used to power our RocksDB
storage engine. Most of the options on this page are pass-through options to the
underlying RocksDB instance, and we change very few of their default settings.

## Pass-through options

### Write buffers

`--rocksdb.write-buffer-size`

The amount of data to build up in each in-memory buffer (backed by a log file)
before closing the buffer and queueing it to be flushed into standard storage.
Default: 64MiB. Larger values may improve performance, especially for bulk
loads.

`--rocksdb.max-write-buffer-number`

The maximum number of write buffers that built up in memory. If this number is
reached before the buffers can be flushed, writes will be slowed or stalled.
Default: 2.

`--rocksdb.min-write-buffer-number-to-merge`

Minimum number of write buffers that will be merged together when flushing to
normal storage. Default: 1.

`--rocksdb.delayed-write-rate` (Hidden)

Limited write rate to DB (in bytes per second) if we are writing to the last
in-memory buffer allowed and we allow more than 3 buffers. Default: 16MiB/s.

### LSM tree structure

`--rocksdb.num-levels`

The number of levels for the database in the LSM tree. Default: 7.

`--rocksdb.num-uncompressed-levels`

The number of levels that do not use compression. The default value is 2.
Levels above this number will use Snappy compression to reduce the disk
space requirements for storing data in these levels.

`--rocksdb.max-bytes-for-level-base` (Hidden)

The maximum total data size in bytes in level-1 of the LSM tree. Default:
256MiB.

`--rocksdb.max-bytes-for-level-multiplier`

The maximum total data size in bytes for level L of the LSM tree can be
calculated as `max-bytes-for-level-base * (max-bytes-for-level-multiplier ^
(L-1))`. Default: 10.

### File I/O

`--rocksdb.compaction-read-ahead-size` (Hidden)

If non-zero, we perform bigger reads when doing compaction. If you're  running
RocksDB on spinning disks, you should set this to at least 2MiB. That way
RocksDB's compaction is doing sequential instead of random reads. Default: 0.

`--rocksdb.use-direct-reads` (Hidden)

Only meaningful on Linux. If set, use `O_DIRECT` for reading files. Default:
false.

`--rocksdb.use-direct-writes` (Hidden)

Only meaningful on Linux. If set,use `O_DIRECT` for writing files. Default:
true.

`--rocksdb.use-fsync` (Hidden)

If set, issue an `fsync` call when writing to disk (set to false to issue
`fdatasync` only. Default: false.

### Background tasks

`--rocksdb.base-background-compactions` (Hidden)

Suggested number of concurrent background compaction jobs, submitted to the low
priority thread pool. Default: 1.

`--rocksdb.max-background-compactions`

Maximum number of concurrent background compaction jobs, submitted to the low
priority thread pool. Default: 1.

`--rocksdb.max-background-flushes`

Maximum number of concurrent flush operations, submitted to the high priority
thread pool. Default: 1.

`--rocksdb.num-threads-priority-high`

Number of threads for high priority operations (e.g. flush). We recommend
setting this equal to `max-background-flushes`. Default: 1.

`--rocksdb.num-threads-priority-low`

Number of threads for low priority operations (e.g. compaction). We recommend
setting this equal to `max-background-compactions`. Default: 1.

### Caching

`--rocksdb.block-cache-size`

This is the size of the block cache in bytes. Increasing this may improve
performance.  If there is less than 4GiB of RAM on the system, the default value
is 256MiB. If there is more, the default is `(system RAM size - 2GiB) * 0.3`.

`--rocksdb.block-cache-shard-bits`

The number of bits used to shard the block cache to allow concurrent operations.
To keep individual shards at a reasonable size (i.e. at least 512KB), keep this
value to at most `block-cache-shard-bits / 512KB`. Default: `block-cache-size /
2^19`.
  
`--rocksdb.table-block-size`

Approximate size of user data (in bytes) packed per block for uncompressed data.

`--rocksdb.recycle-log-file-num` (Hidden)

Number of log files to keep around for recycling. Default: 0.

### Miscellaneous

`--rocksdb.verify-checksums-in-compaction` (Hidden)

If true, compaction will verify the data checksum on every read that happens as
part of compaction. Default: true;

`--rocksdb.optimize-filters-for-hits` (Hidden)

This flag specifies that the implementation should optimize the filters mainly
for cases where keys are found rather than also optimize for the case where
keys are not. This would be used in cases where the application knows that
there are very few misses or the performance in the case of misses is not as
important. Default: false.

`--rocksdb.wal-recovery-skip-corrupted` (Hidden)

If true, skip corrupted records in WAL recovery. Default: false.

## Non-Pass-Through Options

`--rocksdb.wal-file-timeout` (Hidden)

Timeout after which unused WAL files are deleted (in seconds). Default: 10.0s.

Data of ongoing transactions is stored in RAM. Transactions that get too big 
(in terms of number of operations involved or the total size of data created or
modified by the transaction) will be committed automatically. Effectively this 
means that big user transactions are split into multiple smaller RocksDB 
transactions that are committed individually. The entire user transaction will 
not necessarily have ACID properties in this case.
 
The following options can be used to control the RAM usage and automatic 
intermediate commits for the RocksDB engine: 

`--rocksdb.max-transaction-size`

Transaction size limit (in bytes). Transactions store all keys and values in
RAM, so large transactions run the risk of causing out-of-memory sitations.
This setting allows you to ensure that does not happen by limiting the size of
any individual transaction. Transactions whose operations would consume more
RAM than this threshold value will abort automatically with error 32 ("resource
limit exceeded").

`--rocksdb.intermediate-commit-size`

If the size of all operations in a transaction reaches this threshold, the transaction 
is committed automatically and a new transaction is started. The value is specified in bytes.
  
`--rocksdb.intermediate-commit-count`

If the number of operations in a transaction reaches this value, the transaction is 
committed automatically and a new transaction is started.
