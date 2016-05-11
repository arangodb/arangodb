
@startDocuBlock WalLogfileAllowOversizeEntries
@brief whether or not oversize entries are allowed
`--wal.allow-oversize-entries`

Whether or not it is allowed to store individual documents that are bigger
than would fit into a single logfile. Setting the option to false will
make
such operations fail with an error. Setting the option to true will make
such operations succeed, but with a high potential performance impact.
The reason is that for each oversize operation, an individual oversize
logfile needs to be created which may also block other operations.
The option should be set to *false* if it is certain that documents will
always have a size smaller than a single logfile.
@endDocuBlock

