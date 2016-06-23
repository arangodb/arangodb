!CHAPTER General Upgrade Information

!SUBSECTION Recommended major upgrade procedure

To upgrade an existing ArangoDB 2.x to 3.0 please use the procedure described
[here](../../Administration/Upgrading/Upgrading30.md).

!SUBSECTION Recommended minor upgrade procedure

To upgrade an existing ArangoDB database to a newer version of ArangoDB 
(e.g. 3.0 to 3.1, or 3.3 to 3.4), the following method is recommended:

* Check the *CHANGELOG* and the
  [list of incompatible changes](../../ReleaseNotes/UpgradingChanges28.md) for API or
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
unsupported and is likely to cause problems when using 2.3 datafiles
with an ArangoDB 2.2 instance.
