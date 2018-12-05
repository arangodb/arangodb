Community to Enterprise Upgrade Procedure
=========================================

{% hint 'warning' %}
While migrating from the Community to the Enterprise Edition is supported, 
installing directly the Enterprise package over the Community package is **not**
supported. Please see below for the correct migration procedure.
{% endhint %}

{% hint 'danger' %}
Migrating from Enterprise to Community Edition is, in general, **not** supported. This
is because the Community Edition does not include some features, such as 
[SmartGraphs](../Graphs/SmartGraphs/README.md) that, if used while the database
was running under the Enterprise Edition, does not make easily possible the
conversion of some database structures.
{% endhint %}

Upgrading from the Community to the Enterprise Edition requires unistallation of
the Community package (can be done in a way that the database data are preserved)
and installation of the Enterprise package.

Procedure for a _Logical_ Upgrade
---------------------------------

1. Use the tool [_arangodump_ ](../Programs/Arangodump/README.md) to take a backup
   of your _community_ database
1. Uninstall the ArangoDB Community package
1. Install the ArangoDB Enterprise package (and start your _Single Instance_, _Active
   Failover_ or _Cluster_)
1. Restore the backup using the tool [_arangorestore_ ](../Programs/Arangorestore/README.md).

Procedure for an _In-Place_ Upgrade
----------------------------------

1. Uninstall the ArangoDB Community package (make sure this is done in a way that
   your database is kept on your disk, e.g. on _Debian_ systems do **not** use the
   _purge_ option of _dpkg_ or, on Windows, do not check the _Delete databases with
   unistallation?_ option)
1. Install the ArangoDB Enterprise package
1. If you are moving from version A to version B, where B > A, start _arangod_ on
   your data directory with the option _--database.auto-upgrade_ (in addition to
   any other options you are currently using). The server will stop after a while
   (check the log file of _arangod_ as there should be relevant information about
   the upgrade). If you are using a setup that involves several _arangod_ processes
   (e.g. _Active Failover_ or _Cluster_) this step has to be repeated for all _arangod_
   processes
1. Start ArangoDB Enterprise (in the same way you were starting ArangoDB Community)
