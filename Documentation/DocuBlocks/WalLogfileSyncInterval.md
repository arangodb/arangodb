
@startDocuBlock WalLogfileSyncInterval
@brief interval for automatic, non-requested disk syncs
`--wal.sync-interval`

The interval (in milliseconds) that ArangoDB will use to automatically
synchronize data in its write-ahead logs to disk. Automatic syncs will
only
be performed for not-yet synchronized data, and only for operations that
have been executed without the *waitForSync* attribute.
@endDocuBlock

