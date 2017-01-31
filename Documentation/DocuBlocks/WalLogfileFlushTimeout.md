
@startDocuBlock WalLogfileFlushTimeout
@brief WAL flush timeout
`--wal.flush-timeout

The timeout (in milliseconds) that ArangoDB will at most wait when flushing
a full WAL logfile to disk. When the timeout is reached and the flush is
not completed, the operation that requested the flush will fail with a 
*lock timeout* error.
@endDocuBlock

