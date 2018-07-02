General Upgrade Information
===========================

Upgrade Methods
---------------

There are two main ways to upgrade ArangoDB:

- _In-Place_ upgrade: this is a binary upgrade.
- _Logical_ upgrade: when the data is exported from the old ArangoDB version,
   using [_arangodump_ ](..\..\Programs\Arangodump\README.md) and then restored in
   the new ArangoDB version using [_arangorestore_ ](..\..\Programs\Arangorestore\README.md).
   Depending on the size of your database, this strategy can be more time consuming.

Before the Upgrade
------------------

Before upgrading, it is suggested to:

- Check the [CHANGELOG](../../ReleaseNotes/README.md#changelogs) and the
  [list of incompatible changes](../../ReleaseNotes/README.md#incompatible-changes)
  for API or other changes in the new version of ArangoDB and make sure your applications
  can deal with them.
- As an extra precaution, you might want to copy the entire "old" data directory
  to a safe place, after stopping the ArangoDB Server running on it.

Additional Notes
----------------
  
- To upgrade an existing ArangoDB 2.x to 3.0 please use the procedure described
in [Upgrading to 3.0](../VersionSpecific/Upgrading30.md).
