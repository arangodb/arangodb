Downgrading
===========

A direct, in-place downgrade of ArangoDB is **not** supported. If you have upgraded
your ArangoDB package, and then also upgraded your current data directory, it is
not supported to downgrade the package and start an older ArangoDB version on a
data directory that was upgraded already.

If you are using a standalone ArangoDB server, data directory could have been upgraded
automatically during package upgrade. If you are using the _Starter_ to start your
ArangoDB, and you have not triggered yet the rolling upgrade, or upgraded it
manually, your data directory is (probably) still on the old version, so you should
be able to binary downgrade in this case.

Supported Downgrade Procedures
------------------------------

In order to downgrade, the following options are available:

- Restore a backup you took using the tool [Arangodump](../Programs/Arangodump/README.md)
  before the upgrade.
- Start the old package on the data directory backup you took before the upgrade.

### Restore an _arangodump_ Backup

This procedure assumes that you have taken an _arangodump_ backup using the old
ArangoDB version, before you upgraded it. 

1. Stop ArangoDB (if you are using an Active Failover, or a Cluster, stop all the needed
   processes on all the machines).
2. As extra precaution, take a backup of your current data directory (at filesystem level).
   If you are using an Active Failover or a Cluster, you will need to backup all the data
   directories of all the processes involved, from all machines. Make sure you move your
   data directory backup to a safe place.
3. Uninstall the ArangoDB package (use appropriate _purge_ option so your current data
   directory is deleted, if you are using a stand alone instance).
4. Install the old version of ArangoDB.
5. Start ArangoDB. If you are using the _Starter_, please make sure you use a new data
   directory for the _Starter_.
6. Restore the _arangodump_ backup taken before upgrading.

### Start the old package on the data directory backup

This procedure assumes that you have created a copy of your data directory (after having
stopped the ArangoDB process running on it) before the upgrade. If you are running an
Active Failover, or a Cluster, this procedure assumes that you have stopped them before
the upgrade, and that you have taken a copy of their data directories, from all involved
machines.

This procedure cannot be used if you have done a rolling upgrade of your Active Failover
or Cluster setups (because in this case you do not have a copy of the data directories.

1. Stop ArangoDB (if you are using an Active Failover, or a Cluster, stop all the needed
   processes on all the machines).
2. As extra precaution, take a backup of your data directory (at filesystem level). If
   you are using an Active Failover or a Cluster, you will need to backup all the data
   directories of all the processes involved, from all machines. Make sure you move your
   backup to a safe place.
3. Uninstall the ArangoDB package (use appropriate _purge_ option so your current data
   directory is deleted, if you are using a stand alone instance).
4. Install the old version of ArangoDB.
5. Start ArangoDB on the data directory that you have backup-ed up (at filesystem level)
   before the upgrade. As an extra precaution, please first take a new copy of this
   directory and move it to a safe place.

### Other possibilities

If you have upgraded by mistake, and:

- your data directory has been upgraded already
- it is not possible for you to follow any of the
  [Supported Downgrade Procedures](#supported-downgrade-procedures) because:
  - you do not have a _dump_ backup taken using the old ArangoDB version
  - you do not have a copy of your data directory taken after stopping the old ArangoDB
    process and before the upgrade

...one possible option to downgrade could be to export the data from the new ArangoDB version
using the tool _arangoexport_ and reimport it using the tool _arangoimport_ in the old
version (after having installed and started it on a clean data directory). This method will
require some manual work to recreate the structure of your collections and your indices - but
it might still help you solving an otherwise challenging situation.
