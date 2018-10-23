Backup and Restore
==================

Backup and restore can be done via the tools
[_arangodump_](../Programs/Arangodump/README.md) and
[_arangorestore_](../Programs/Arangorestore/README.md).

Performing frequent backups is important and a recommended best practices that
can allow you to recover your data in case unexpected problems occur.
Hardware failures, system crashes, or users mistakenly deleting data can always
happen. Furthermore, while a big effort is put into the development and testing
of ArangoDB (in all its deployment modes), ArangoDB, as any other software
product, might include bugs or errors and data loss could occur.
It is therefore important to regularly backup your data to be able to recover
and get up an running again in case of serious problems.

Creating backups of your data before an ArangoDB upgrade is also a best practice.

{% hint 'warning' %}
Making use of a high availability deployment mode of ArangoDB, like Active Failover,
Cluster or data-center to data-center replication, does not remove the need of
taking frequent backups, which are recommended also when using such deployment modes.
{% endhint %}

<!-- Offline dumps -->

<!-- Hot backups  -->

<!-- Cluster -->
