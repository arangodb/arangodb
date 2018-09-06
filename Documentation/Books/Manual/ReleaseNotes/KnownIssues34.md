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

ArangoSearch
------------

* ArangoSearch ingores `_id` attribute even if `includeAllFields` is set to `true` (internal #445)
* Creation of ArangoSearch on a large collection may cause OOM (internal #407)
* Long-running transaction with a huge number of DML operations may cause OOM (internal #407)
* Using score functions (BM25/TFIDF) in ArangoDB expression is not supported (internal #316)
* Using a loop variable in expressions within a corresponding SEARCH condition is not supported (internal #318)
* Data of "NONE" collection could be accessed via a view for a used regardless of rights check (internal #453)
* "NONE" read permission is not checked for a single link in a view with multiple links where others are "RW" for a user in cluster (internal #452)
* ArangoSearch doesn't support joins with satellite collections (internal #440)
* RocksDB recovery fails sometimes after renaming a view (internal #469)


