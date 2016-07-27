

@brief throttle writes to WAL when at least such many operations are
waiting for garbage collection
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
