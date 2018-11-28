# ArangoDB Server RocksDB Options

RocksDB is a highly configurable key-value store used to power our RocksDB
storage engine. Most of the options on this page are pass-through options to the
underlying RocksDB instance, and we change very few of their default settings.

Depending on the [storage engine you have chosen](Server.md#storage-engine)
the availability and the scope of these options changes. 

In case you have chosen `mmfiles` some of the following options apply to
persistent indexes.
In case of `rocksdb` it will apply to all data stored as well as indexes.

## Pass-through options

`--rocksdb.wal-directory`

Absolute path for the RocksDB WAL files. If left empty, this will use a
subdirectory `journals` inside the data directory.

### Write buffers

`--rocksdb.write-buffer-size`

The amount of data to build up in each in-memory buffer (backed by a log file)
before closing the buffer and queuing it to be flushed into standard storage.
Default: 64MiB. Larger values may improve performance, especially for bulk
loads.

`--rocksdb.max-write-buffer-number`

The maximum number of write buffers that built up in memory. If this number is
reached before the buffers can be flushed, writes will be slowed or stalled.
Default: 2.

`--rocksdb.total-write-buffer-size`

The total amount of data to build up in all in-memory buffers (backed by log
files). This option, together with the block cache size configuration option,
can be used to limit memory usage. If set to 0, the memory usage is not limited.

If set to a value larger than 0, this will cap memory usage for write buffers 
but may have an effect on performance. If there is less than 4GiB of RAM on the 
system, the default value is 512MiB. If there is more, the default is 
`(system RAM size - 2GiB) * 0.5`.

`--rocksdb.min-write-buffer-number-to-merge`

Minimum number of write buffers that will be merged together when flushing to
normal storage. Default: 1.

`--rocksdb.max-total-wal-size`

Maximum total size of WAL files that, when reached, will force a flush of all
column families whose data is backed by the oldest WAL files. Setting this
to a low value will trigger regular flushing of column family data from memtables, 
so that WAL files can be moved to the archive.
Setting this to a high value will avoid regular flushing but may prevent WAL
files from being moved to the archive and being removed.

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

`--rocksdb.dynamic-level-bytes`

If true, the amount of data in each level of the LSM tree is determined
dynamically so as to minimize the space amplification; otherwise, the level
sizes are fixed. The dynamic sizing allows RocksDB to maintain a well-structured
LSM tree regardless of total data size. Default: true.

`--rocksdb.max-bytes-for-level-base`

The maximum total data size in bytes in level-1 of the LSM tree. Only effective
if `--rocksdb.dynamic-level-bytes` is false. Default: 256MiB.

`--rocksdb.max-bytes-for-level-multiplier`

The maximum total data size in bytes for level L of the LSM tree can be
calculated as `max-bytes-for-level-base * (max-bytes-for-level-multiplier ^
(L-1))`. Only effective if `--rocksdb.dynamic-level-bytes` is false. Default:
10.

`--rocksdb.level0-compaction-trigger`

Compaction of level-0 to level-1 is triggered when this many files exist in
level-0. Setting this to a higher number may help bulk writes at the expense of
slowing down reads. Default: 2.

`--rocksdb.level0-slowdown-trigger`

When this many files accumulate in level-0, writes will be slowed down to
`--rocksdb.delayed-write-rate` to allow compaction to catch up. Default: 20.

`--rocksdb.level0-stop-trigger`

When this many files accumulate in level-0, writes will be stopped to allow
compaction to catch up. Default: 36.

### File I/O

`--rocksdb.compaction-read-ahead-size`

If non-zero, we perform bigger reads when doing compaction. If you're  running
RocksDB on spinning disks, you should set this to at least 2MiB. That way
RocksDB's compaction is doing sequential instead of random reads. Default: 0.

`--rocksdb.use-direct-reads` (Hidden)

Only meaningful on Linux. If set, use `O_DIRECT` for reading files. Default:
false.

`--rocksdb.use-direct-io-for-flush-and-compaction` (Hidden)

Only meaningful on Linux. If set, use `O_DIRECT` for writing files. Default: false.

`--rocksdb.use-fsync` (Hidden)

If set, issue an `fsync` call when writing to disk (set to false to issue
`fdatasync` only. Default: false.
  
`--rocksdb.block-align-data-blocks`

If true, data blocks are aligned on the lesser of page size and block size,
which may waste some memory but may reduce the number of cross-page I/O operations.

### Background tasks

`--rocksdb.max-background-jobs`

Maximum number of concurrent background compaction jobs, submitted to the low
priority thread pool. Default: number of processors.

`--rocksdb.num-threads-priority-high`

Number of threads for high priority operations (e.g. flush). We recommend
setting this equal to `max-background-flushes`. Default: number of processors / 2.

`--rocksdb.num-threads-priority-low`

Number of threads for low priority operations (e.g. compaction). Default: number of processors / 2.

### Caching

`--rocksdb.block-cache-size`

This is the maximum size of the block cache in bytes. Increasing this may improve
performance.  If there is less than 4GiB of RAM on the system, the default value
is 256MiB. If there is more, the default is `(system RAM size - 2GiB) * 0.3`.

`--rocksdb.enforce-block-cache-size-limit`

Whether or not the maximum size of the RocksDB block cache is strictly enforced.
This option can be set to limit the memory usage of the block cache to at most the
specified size. If then inserting a data block into the cache would exceed the 
cache's capacity, the data block will not be inserted. If the flag is not set,
a data block may still get inserted into the cache. It is evicted later, but the
cache may temporarily grow beyond its capacity limit. 

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

`--rocksdb.optimize-filters-for-hits` (Hidden)

This flag specifies that the implementation should optimize the filters mainly
for cases where keys are found rather than also optimize for the case where
keys are not. This would be used in cases where the application knows that
there are very few misses or the performance in the case of misses is not as
important. Default: false.

`--rocksdb.wal-recovery-skip-corrupted` (Hidden)

If true, skip corrupted records in WAL recovery. Default: false.

## Non-Pass-Through Options

`--rocksdb.wal-file-timeout`

Timeout after which unused WAL files are deleted (in seconds). Default: 10.0s.

Data of ongoing transactions is stored in RAM. Transactions that get too big
(in terms of number of operations involved or the total size of data created or
modified by the transaction) will be committed automatically. Effectively this
means that big user transactions are split into multiple smaller RocksDB
transactions that are committed individually. The entire user transaction will
not necessarily have ACID properties in this case.

The following options can be used to control the RAM usage and automatic
intermediate commits for the RocksDB engine:

`--rocksdb.wal-file-timeout-initial` (Hidden)

Timeout after which deletion of unused WAL files kicks in after server start
(in seconds). Default: 180.0s

By decreasing this option's value, the server will start the removal of obsolete
WAL files earlier after server start. This is useful in testing environments that
are space-restricted and do not require keeping much WAL file data at all.

`--rocksdb.max-transaction-size`

Transaction size limit (in bytes). Transactions store all keys and values in
RAM, so large transactions run the risk of causing out-of-memory situations.
This setting allows you to ensure that does not happen by limiting the size of
any individual transaction. Transactions whose operations would consume more
RAM than this threshold value will abort automatically with error 32 ("resource
limit exceeded").

`--rocksdb.intermediate-commit-size`

If the size of all operations in a transaction reaches this threshold, the
transaction is committed automatically and a new transaction is started. The
value is specified in bytes.

`--rocksdb.intermediate-commit-count`

If the number of operations in a transaction reaches this value, the transaction
is committed automatically and a new transaction is started.

`--rocksdb.throttle`

If enabled, throttles the ingest rate of writes if necessary to reduce chances 
of compactions getting too far behind and blocking incoming writes. This option
is `true` by default.

`--rocksdb.sync-interval`

The interval (in milliseconds) that ArangoDB will use to automatically
synchronize data in RocksDB's write-ahead logs to disk. Automatic syncs will
only be performed for not-yet synchronized data, and only for operations that
have been executed without the *waitForSync* attribute.

Note: this option is not supported on Windows platforms. Setting the option to
a value greater 0 will produce a startup warning.

`--rocksdb.use-file-logging`

When set to *true*, enables writing of RocksDB's own informational LOG files into 
RocksDB's database directory.

This option is turned off by default, but can be enabled for debugging RocksDB
internals and performance.

`--rocksdb.debug-logging`

When set to *true*, enables verbose logging of RocksDB's actions into the logfile
written by ArangoDB (if option `--rocksdb.use-file-logging` is off) or RocksDB's
own log (if option `--rocksdb.use-file-logging` is on).

This option is turned off by default, but can be enabled for debugging RocksDB
internals and performance.
