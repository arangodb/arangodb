

@brief flushes the currently open WAL logfile
`internal.wal.flush(waitForSync, waitForCollector)`

Flushes the write-ahead log. By flushing the currently active write-ahead
logfile, the data in it can be transferred to collection journals and
datafiles. This is useful to ensure that all data for a collection is
present in the collection journals and datafiles, for example, when dumping
the data of a collection.

The *waitForSync* option determines whether or not the operation should
block until the not-yet synchronized data in the write-ahead log was
synchronized to disk.

The *waitForCollector* operation can be used to specify that the operation
should block until the data in the flushed log has been collected by the
write-ahead log garbage collector. Note that setting this option to *true*
might block for a long time if there are long-running transactions and
the write-ahead log garbage collector cannot finish garbage collection.

@EXAMPLES

@EXAMPLE_ARANGOSH_OUTPUT{WalFlush}
  require("internal").wal.flush();
@END_EXAMPLE_ARANGOSH_OUTPUT

