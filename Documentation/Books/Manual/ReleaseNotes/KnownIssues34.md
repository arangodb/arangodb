Known Issues
============

The following known issues are present in this version of ArangoDB and will be fixed
in follow-up releases:

Installer
---------

* Rolling upgrades are not yet supported with the RC version of ArangoDB 3.4, when
  there are multiple instances of the same ArangoDB version running on the same host,
  and only some of them shall be upgraded, or they should be upgraded one after the
  other.
* The windows installer doesn't offer a way to continue the installation if it failed
  because of a locked database.

Cluster
-------

* sometimes the shutdown order in the cluster will cause the shutdown to hang infinitely.
