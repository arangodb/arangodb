Switching the storage engine
----------------------------

In order to use a different storage engine with an existing data directory,
it is required to first create a logical backup of the data using the 
tool [_arangodump_](../../Programs/Arangodump/README.md).

After that, the _arangod_ server process should be restarted with the desired storage
engine selected (this can be done by setting the option *--server.storage-engine*) 
and using a non-existing data directory.

When the server is up and running with the desired storage engine, the data
can be re-imported using the tool
[_arangorestore_](../../Programs/Arangorestore/README.md).
