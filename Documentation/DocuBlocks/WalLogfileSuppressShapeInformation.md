
@startDocuBlock WalLogfileSuppressShapeInformation
@brief suppress shape information
`--wal.suppress-shape-information`

Setting this variable to *true* will lead to no shape information being
written into the write-ahead logfiles for documents or edges. While this
is
a good optimization for a single server to save memory (and disk space),
it
it will effectively disable using the write-ahead log as a reliable source
for replicating changes to other servers. A master server with this option
set to *true* will not be able to fully reproduce the structure of saved
documents after a collection has been deleted. In case a replication
client
requests a document for which the collection is already deleted, the
master
will return an empty document. Note that this only affects replication and
not normal operation on the master.

**Do not set this variable to *true* on a server that you plan to use as a
replication master**
@endDocuBlock

