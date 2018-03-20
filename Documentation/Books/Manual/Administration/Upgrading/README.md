General Upgrade Information
===========================

Recommended major upgrade procedure
-----------------------------------

To upgrade an existing ArangoDB 2.x to 3.0 please use the procedure described
[here](../../Administration/Upgrading/Upgrading30.md).

Recommended minor upgrade procedure
-----------------------------------

To upgrade an existing ArangoDB database to a newer version of ArangoDB 
(e.g. 3.0 to 3.1, or 3.1 to 3.2), the following method is recommended:

* Check the *CHANGELOG* and the
  [list of incompatible changes](../../ReleaseNotes/UpgradingChanges32.md) for API or
  other changes in the new version of ArangoDB and make sure your applications
  can deal with them
* Stop the "old" arangod service or binary
* Copy the entire "old" data directory to a safe place (that is, a backup)
* Install the new version of ArangoDB and start the server with
  the *--database.auto-upgrade* option once. This might write to the logfile of ArangoDB,
  so you may want to check the logs for any issues before going on.
* Start the "new" arangod service or binary regularly and check the logs for any
  issues. When you're confident everything went well, you may want to check the
  database directory for any files with the ending *.old*. These files are
  created by ArangoDB during upgrades and can be safely removed manually later.

If anything goes wrong during or shortly after the upgrade:

* Stop the "new" arangod service or binary
* Revert to the "old" arangod binary and restore the "old" data directory
* Start the "old" version again

It is not supported to use datafiles created or modified by a newer
version of ArangoDB with an older ArangoDB version. For example, it is
unsupported and is likely to cause problems when using 3.2 datafiles
with an ArangoDB 3.0 instance.

Switching the storage engine
----------------------------

In order to use a different storage engine with an existing data directory,
it is required to first create a logical backup of the data using *arangodump*.

After that, the arangod server should be restarted with the desired storage
engine selected (this can be done by setting the option *--server.storage-engine*) 
and using a non-existing data directory.

When the server is up and running with the desired storage engine, the data
can be re-imported using *arangorestore*.
