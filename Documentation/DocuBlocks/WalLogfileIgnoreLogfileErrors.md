

@brief ignore logfile errors when opening logfiles
`--wal.ignore-logfile-errors`

Ignores any recovery errors caused by corrupted logfiles on startup. When
set to *false*, the recovery procedure on startup will fail with an error
whenever it encounters a corrupted (that includes only half-written)
logfile. This is a security precaution to prevent data loss in case of
disk
errors etc. When the recovery procedure aborts because of corruption, any
corrupted files can be inspected and fixed (or removed) manually and the
server can be restarted afterwards.

Setting the option to *true* will make the server continue with the
recovery
procedure even in case it detects corrupt logfile entries. In this case it
will stop at the first corrupted logfile entry and ignore all others,
which
might cause data loss.

