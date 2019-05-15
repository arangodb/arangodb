
@startDocuBlock put_admin_wal_flush
@brief Sync the WAL to disk.

@RESTHEADER{PUT /_admin/wal/flush, Flushes the write-ahead log, RestWalHandler:flush}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{waitForSync,boolean,optional}
Whether or not the operation should block until the not-yet synchronized
data in the write-ahead log was synchronized to disk.

@RESTQUERYPARAM{waitForCollector,boolean,optional}
Whether or not the operation should block until the data in the flushed
log has been collected by the write-ahead log garbage collector. Note that
setting this option to *true* might block for a long time if there are
long-running transactions and the write-ahead log garbage collector cannot
finish garbage collection.

@RESTDESCRIPTION
Flushes the write-ahead log. By flushing the currently active write-ahead
logfile, the data in it can be transferred to collection journals and
datafiles. This is useful to ensure that all data for a collection is
present in the collection journals and datafiles, for example, when dumping
the data of a collection.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Is returned if the operation succeeds.

@RESTRETURNCODE{405}
is returned when an invalid HTTP method is used.
@endDocuBlock

