
@startDocuBlock WalLogfileHistoricLogfiles
@brief maximum number of historic logfiles
`--wal.historic-logfiles`

The maximum number of historic logfiles that ArangoDB will keep after they
have been garbage-collected. If no replication is used, there is no need
to keep historic logfiles except for having a local changelog.

In a replication setup, the number of historic logfiles affects the amount
of data a slave can fetch from the master's logs. The more historic
logfiles, the more historic data is available for a slave, which is useful
if the connection between master and slave is unstable or slow. Not having
enough historic logfiles available might lead to logfile data being
deleted
on the master already before a slave has fetched it.
@endDocuBlock

