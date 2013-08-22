General upgrade information {#UpgradingGeneral}
===============================================

@NAVIGATE_UpgradingGeneral
@EMBEDTOC{UpgradingGeneralTOC}

Recommended upgrade procedure {#UpgradingGeneralIntroduction}
=============================================================

To upgrade an existing ArangoDB database to a newer version of ArangoDB (e.g. 1.2 to 1.3, or 1.3 to 1.4), the following method is recommended:
- Check the CHANGELOG for API or other changes in the new version of ArangoDB, and make sure your applications can deal with them
- Stop the "old" arangod service or binary
- Copy the entire "old" data directory to a safe place (that is, a backup)
- Install the new version of ArangoDB and start the server with the `--upgrade` option once. This might write to the logfile of ArangoDB, so you may want to check the logs for any issues before going on.
- Start the "new" arangod service or binary regularly and check the logs for any issues. When you're confident everything went well, you may want to check the database directory for any files with the ending `.old`. These files are created by ArangoDB during upgrades and can be safely removed manually later.

If anything goes wrong during or shortly after the upgrade:
- Stop the "new" arangod service or binary
- Revert to the "old" arangod binary and restore the "old" data directory
- Start the "old" version again

It is not supported to use datafiles created or modified by a newer version of ArangoDB with an older ArangoDB version. For example, it is unsupported and is likely to cause problems when using 1.4 datafiles with an ArangoDB 1.3 instance.
