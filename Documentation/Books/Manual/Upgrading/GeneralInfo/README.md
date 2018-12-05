General Upgrade Information
===========================

Upgrade Methods
---------------

There are two main ways to upgrade ArangoDB:

- _In-Place_ upgrade: when the installed ArangoDB package is replaced with the new one, and
  the new ArangoDB binary is started on the existing data directory.
- _Logical_ upgrade: when the data is exported from the old ArangoDB version,
  using [_arangodump_ ](..\..\Programs\Arangodump\README.md) and then restored in
  the new ArangoDB version using [_arangorestore_ ](..\..\Programs\Arangorestore\README.md).
  Depending on the size of your database, this strategy can be more time consuming,
  but needed in some circumstances.

Before the Upgrade
------------------

Before upgrading, it is recommended to:

- Check the [CHANGELOG](../../ReleaseNotes/README.md#changelogs) and the
  [list of incompatible changes](../../ReleaseNotes/README.md#incompatible-changes)
  for API or other changes in the new version of ArangoDB, and make sure your applications
  can deal with them.
- As an extra precaution, and as a requirement if you want to [downgrade](../../Downgrading/README.md),
  you might want to:
  - Take a backup of the old ArangoDB database, using [Arangodump](../../Programs/Arangodump/README.md),
    as well as
  - Copy the entire "old" data directory to a safe place, after stopping the ArangoDB Server
    running on it (if you are running an Active Failover, or a Cluster, you will need to take
    a copy of their data directories, from all involved machines, after stopping all the running
    ArangoDB processes).

Upgrade Paths
-------------

- It is always possible to upgrade between hot-fixes of the same GA release, i.e
  from X.Y.w to X.Y.z, where z>w.
  - Examples: 
    - Upgrading from 3.4.0 to 3.4.1 or (directly to) 3.4.2 is supported.
    - Upgrading from 3.3.7 to 3.3.8 or (directly to) 3.3.11 is supported.
    - Upgrading from 3.2.12 to 3.2.13 or (directly to) 3.2.15 is supported.
- It possible to upgrade between two different consecutive GA releases, but it is
  not officially supported to upgrade if the two GA releases are not consecutive
  (in this case, you first have to upgrade to all intermediate releases).
  - Examples:
    - Upgrading from 3.3 to 3.4 is supported.
    - Upgrading from 3.2 to 3.3 is supported.
    - Upgrading from 3.2 to 3.4 directly is not officially supported: the officially
      supported upgrade path in this case is 3.2 to 3.3, and then 3.3 to 3.4.	  
  - **Important:** before upgrading between two consecutive GA releases it is highly recommended
    to first upgrade the previous GA release to its latest hot-fix.
    - Examples: 
      - To upgrade from 3.2 to 3.3, first upgrade your 3.2 installation to 3.2.latest.
      - To upgrade from 3.3 to 3.4, first upgrade your 3.3 installation to 3.3.latest.
	  
### Additional Notes Regarding Rolling Upgrades

In addition to the paragraph above, rolling upgrades via the tool _Starter_ are supported,
as documented in the _Section_ [Upgrading Starter Deployments](../Starter/README.md),
with the following limitations:

- Rolling upgrades between 3.3 and 3.4 are not supported before 3.3.20 and 3.4.0.
- Rolling upgrades between 3.2 and 3.3 are not supported before 3.2.15 and 3.3.9.
  
