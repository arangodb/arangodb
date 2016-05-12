
@startDocuBlock WalLogfileSize
@brief the size of each WAL logfile
`--wal.logfile-size`

Specifies the filesize (in bytes) for each write-ahead logfile. The
logfile
size should be chosen so that each logfile can store a considerable amount
of
documents. The bigger the logfile size is chosen, the longer it will take
to fill up a single logfile, which also influences the delay until the
data
in a logfile will be garbage-collected and written to collection journals
and datafiles. It also affects how long logfile recovery will take at
server start.
@endDocuBlock

