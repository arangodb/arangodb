

@brief maximum number of slots to be used in parallel
`--wal.slots`

Configures the amount of write slots the write-ahead log can give to write
operations in parallel. Any write operation will lease a slot and return
it
to the write-ahead log when it is finished writing the data. A slot will
remain blocked until the data in it was synchronized to disk. After that,
a slot becomes reusable by following operations. The required number of
slots is thus determined by the parallelity of write operations and the
disk synchronization speed. Slow disks probably need higher values, and
fast
disks may only require a value lower than the default.

