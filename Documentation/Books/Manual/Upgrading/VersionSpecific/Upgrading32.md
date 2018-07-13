Upgrading to ArangoDB 3.2
=========================

Please read the following sections if you upgrade from a previous
version to ArangoDB 3.2. Also, please, be sure that you have checked the list
of [incompatible changes in 3.2](../../ReleaseNotes/UpgradingChanges32.md) before
upgrading.

Switching the storage engine
----------------------------

In order to use a different storage engine with an existing data directory,
it is required to first create a logical backup of the data using *arangodump*.
That backup should be created before the upgrade to 3.2.

After that, the ArangoDB installation can be upgraded and stopped. The server
should then be restarted with the desired storage engine selected (this can be 
done by setting the option *--server.storage-engine*) and using a non-existing 
data directory. This will start the server with the selected storage engine
but with no data.

When the server is up and running, the data from the logical backup can be 
re-imported using *arangorestore*.
