
@startDocuBlock WalLogfileReserveLogfiles
@brief maximum number of reserve logfiles
`--wal.reserve-logfiles`

The maximum number of reserve logfiles that ArangoDB will create in a
background process. Reserve logfiles are useful in the situation when an
operation needs to be written to a logfile but the reserve space in the
logfile is too low for storing the operation. In this case, a new logfile
needs to be created to store the operation. Creating new logfiles is
normally slow, so ArangoDB will try to pre-create logfiles in a background
process so there are always reserve logfiles when the active logfile gets
full. The number of reserve logfiles that ArangoDB keeps in the background
is configurable with this option.
@endDocuBlock

