Switching the storage engine
----------------------------

In order to use a different storage engine with an existing data directory,
it is required to first create a logical backup of the data using the 
tool [_arangodump_](../../Programs/Arangodump/README.md).

After that, the _arangod_ server process should be restarted with the desired storage
engine selected (this can be done by setting the option *--server.storage-engine*,
or by editing the configuartion file of the server) and using a **non-existing data directory**.
If you have deployed using the [_Starter_](../../Programs/Starter/README.md),
instead of _arangod_ you will need to run _arangodb_, and pass to it the option 
*--server.storage-engine* and the option *--starter.data-dir* to set a new
data directory.

When the server is up and running with the desired storage engine, the data
can be re-imported using the tool
[_arangorestore_](../../Programs/Arangorestore/README.md).

For a list of available storage engines, and more information on their
differences, please refer to the [Storage Engines](../../Architecture/StorageEngines.md)
page under the [Architecture](../../Architecture/README.md) chapter.
