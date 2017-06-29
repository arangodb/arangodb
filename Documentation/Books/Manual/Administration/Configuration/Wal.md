MMFiles Write-ahead log options
===============================

Since ArangoDB 2.2, the MMFiles storage engine will write all data-modification
operations into its write-ahead log.

With ArangoDB 3.2 another Storage engine option becomes available - [RocksDB](RocksDB.md).
In case of using RocksDB most of the subsequent options don't have a useful meaning.

The write-ahead log is a sequence of logfiles that are written in an append-only
fashion. Full logfiles will eventually be garbage-collected, and the relevant data
might be transferred into collection journals and datafiles. Unneeded and already
garbage-collected logfiles will either be deleted or kept for the purpose of keeping
a replication backlog.

### Directory

<!-- arangod/Wal/LogfileManager.h -->

The WAL logfiles directory: `--wal.directory`

Specifies the directory in which the write-ahead logfiles should be
stored. If this option is not specified, it defaults to the subdirectory
*journals* in the server's global database directory. If the directory is
not present, it will be created.

### Logfile size

<!-- arangod/Wal/LogfileManager.h -->
@startDocuBlock WalLogfileSize

### Allow oversize entries

<!-- arangod/Wal/LogfileManager.h -->
@startDocuBlock WalLogfileAllowOversizeEntries

### Number of reserve logfiles

<!-- arangod/Wal/LogfileManager.h -->
@startDocuBlock WalLogfileReserveLogfiles

### Number of historic logfiles

<!-- arangod/Wal/LogfileManager.h -->
@startDocuBlock WalLogfileHistoricLogfiles

### Sync interval

<!-- arangod/Wal/LogfileManager.h -->
@startDocuBlock WalLogfileSyncInterval

### Flush timeout

<!-- arangod/Wal/LogfileManager.h -->
@startDocuBlock WalLogfileFlushTimeout

### Throttling

<!-- arangod/Wal/LogfileManager.h -->

Throttle writes to WAL when at least such many operations are
waiting for garbage collection:
`--wal.throttle-when-pending`

The maximum value for the number of write-ahead log garbage-collection
queue elements. If set to *0*, the queue size is unbounded, and no
write-throttling will occur. If set to a non-zero value, write-throttling
will automatically kick in when the garbage-collection queue contains at
least as many elements as specified by this option.
While write-throttling is active, data-modification operations will
intentionally be delayed by a configurable amount of time. This is to
ensure the write-ahead log garbage collector can catch up with the
operations executed.
Write-throttling will stay active until the garbage-collection queue size
goes down below the specified value.
Write-throttling is turned off by default.

`--wal.throttle-wait`

This option determines the maximum wait time (in milliseconds) for
operations that are write-throttled. If write-throttling is active and a
new write operation is to be executed, it will wait for at most the
specified amount of time for the write-ahead log garbage-collection queue
size to fall below the throttling threshold. If the queue size decreases
before the maximum wait time is over, the operation will be executed
normally. If the queue size does not decrease before the wait time is
over, the operation will be aborted with an error.
This option only has an effect if `--wal.throttle-when-pending` has a
non-zero value, which is not the default.

### Number of slots

<!-- arangod/Wal/LogfileManager.h -->

Maximum number of slots to be used in parallel:
`--wal.slots`

Configures the amount of write slots the write-ahead log can give to write
operations in parallel. Any write operation will lease a slot and return
it to the write-ahead log when it is finished writing the data. A slot will
remain blocked until the data in it was synchronized to disk. After that,
a slot becomes reusable by following operations. The required number of
slots is thus determined by the parallelity of write operations and the
disk synchronization speed. Slow disks probably need higher values, and
fast disks may only require a value lower than the default.

### Ignore logfile errors

<!-- arangod/Wal/LogfileManager.h -->

Ignore logfile errors when opening logfiles:
`--wal.ignore-logfile-errors`

Ignores any recovery errors caused by corrupted logfiles on startup. When
set to *false*, the recovery procedure on startup will fail with an error
whenever it encounters a corrupted (that includes only half-written)
logfile. This is a security precaution to prevent data loss in case of disk
errors etc. When the recovery procedure aborts because of corruption, any
corrupted files can be inspected and fixed (or removed) manually and the
server can be restarted afterwards.

Setting the option to *true* will make the server continue with the recovery
procedure even in case it detects corrupt logfile entries. In this case it
will stop at the first corrupted logfile entry and ignore all others, which
might cause data loss.

### Ignore recovery errors

<!-- arangod/Wal/LogfileManager.h -->

Ignore recovery errors:
`--wal.ignore-recovery-errors`

Ignores any recovery errors not caused by corrupted logfiles but by logical
errors. Logical errors can occur if logfiles or any other server datafiles
have been manually edited or the server is somehow misconfigured.

### Ignore (non-WAL) datafile errors

<!-- arangod/RestServer/ArangoServer.h -->

Ignore datafile errors when loading collections:
`--database.ignore-datafile-errors boolean`

If set to `false`, CRC mismatch and other errors in collection datafiles
will lead to a collection not being loaded at all. The collection in this
case becomes unavailable. If such collection needs to be loaded during WAL
recovery, the WAL recovery will also abort (if not forced with option
`--wal.ignore-recovery-errors true`).

Setting this flag to `false` protects users from unintentionally using a
collection with corrupted datafiles, from which only a subset of the
original data can be recovered. Working with such collection could lead
to data loss and follow up errors.
In order to access such collection, it is required to inspect and repair
the collection datafile with the datafile debugger (arango-dfdb).

If set to `true`, CRC mismatch and other errors during the loading of a
collection will lead to the datafile being partially loaded, up to the
position of the first error. All data up to until the invalid position
will be loaded. This will enable users to continue with collection
datafiles
even if they are corrupted, but this will result in only a partial load
of the original data and potential follow up errors. The WAL recovery
will still abort when encountering a collection with a corrupted datafile,
at least if `--wal.ignore-recovery-errors` is not set to `true`.

The default value is *false*, so collections with corrupted datafiles will
not be loaded at all, preventing partial loads and follow up errors. However,
if such collection is required at server startup, during WAL recovery, the
server will abort the recovery and refuse to start.
