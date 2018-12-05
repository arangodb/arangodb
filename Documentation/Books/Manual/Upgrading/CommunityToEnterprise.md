Community to Enterprise Upgrade Procedure
=========================================

Installing the Enterprise package over the Community package is **not** supported.

Upgrading from the Community to the Enterprise Edition requires unistallation of
the Community package (can be done in a way that the database data are preserved)
and installation of the Enterprise package. 

Procedure for a _Logical_ Upgrade
---------------------------------

1. Use the tool [_arangodump_ ](..\..\Programs\Arangodump\README.md) to take a backup
   of your _commynity_ database
1. Uninstall the ArangoDB Community package
1. Install the ArangoDB Enterprise package (and start your Single Instance, Active
   Failover or Cluster)
1. Restore the backup using the tool [_arangorestore_ ](..\..\Programs\Arangorestore\README.md).

Procedure for a _In-Place_ Upgrade
----------------------------------

1. Uninstall the ArangoDB Community package (make sure this is done in a way that
   your database is kept on your disk, e.g. on _Debian_ system do **not** use the
   _purge_ option of _dpkg_)
1. Install the ArangoDB Enterprise package
1. If you are moving from version A to version B, where B > A, start _arangod_ on
   your data directory with the option _--database.auto-upgrade_ (in addition to
   any other options you are currently using). The server will stop after a while
   (check the log file of _arangod_ as there should be relevant information about
   the upgrade). If you are using a setup that involves several _arangod_ processes
   (e.g. _Active Failover_ or _Cluster_) this step has to be repeated for all _arangod_
   processes
1. Start ArangoDB Enterprise
