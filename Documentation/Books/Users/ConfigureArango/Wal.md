Write-ahead log options
=======================

Since ArangoDB 2.2, the server will write all data-modification operations into its
write-ahead log.

The write-ahead log is a sequence of logfiles that are written in an append-only
fashion. Full logfiles will eventually be garbage-collected, and the relevant data
might be transferred into collection journals and datafiles. Unneeded and already
garbage-collected logfiles will either be deleted or kept for the purpose of keeping
a replication backlog.

### Directory
<!-- arangod/Wal/LogfileManager.h -->
@startDocuBlock WalLogfileDirectory

### Logfile size
<!-- arangod/Wal/LogfileManager.h -->
@startDocuBlock WalLogfileSize

### Allow oversize entries
<!-- arangod/Wal/LogfileManager.h -->
@startDocuBlock WalLogfileAllowOversizeEntries

### Suppress shape information
<!-- arangod/Wal/LogfileManager.h -->
@startDocuBlock WalLogfileSuppressShapeInformation

### Number of reserve logfiles
<!-- arangod/Wal/LogfileManager.h -->
@startDocuBlock WalLogfileReserveLogfiles

### Number of historic logfiles
<!-- arangod/Wal/LogfileManager.h -->
@startDocuBlock WalLogfileHistoricLogfiles

### Sync interval
<!-- arangod/Wal/LogfileManager.h -->
@startDocuBlock WalLogfileSyncInterval

### Throttling
<!-- arangod/Wal/LogfileManager.h -->
@startDocuBlock WalLogfileThrottling

### Number of slots
<!-- arangod/Wal/LogfileManager.h -->
@startDocuBlock WalLogfileSlots

### Ignore logfile errors
<!-- arangod/Wal/LogfileManager.h -->
@startDocuBlock WalLogfileIgnoreLogfileErrors

### Ignore recovery errors
<!-- arangod/Wal/LogfileManager.h -->
@startDocuBlock WalLogfileIgnoreRecoveryErrors

### Ignore (non-WAL) datafile errors
<!-- arangod/RestServer/ArangoServer.h -->
@startDocuBlock databaseIgnoreDatafileErrors

