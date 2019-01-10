Known Issues in ArangoDB 3.5
============================

This page lists important issues affecting the 3.5.x versions of the ArangoDB suite of products.
It is not a list of all open issues.

Critical issues (ArangoDB Technical & Security Alerts) are also found at [arangodb.com/alerts](https://www.arangodb.com/alerts/).

ArangoSearch
------------

| Issue      |
|------------|
| **Date Added:** 2018-12-19 <br> **Component:** ArangoSearch <br> **Deployment Mode:** Single-server <br> **Description:** Value of `_id` attribute indexed by ArangoSearch view may become inconsistent after renaming a collection <br> **Affected Versions:** >= 3.5.0 <br> **Fixed in Versions:** - <br> **Reference:** [arangodb/backlog#514](https://github.com/arangodb/backlog/issues/514) (internal) |
| **Date Added:** 2018-12-03 <br> **Component:** ArangoSearch <br> **Deployment Mode:** Cluster <br> **Description:** Score values evaluated by corresponding score functions (BM25/TFIDF) may differ in single-server and cluster with a collection having more than 1 shard <br> **Affected Versions:** 3.4.0-RC.5 <br> **Fixed in Versions:** - <br> **Reference:** [arangodb/backlog#508](https://github.com/arangodb/backlog/issues/508) (internal) |
| **Date Added:** 2018-12-03 <br> **Component:** ArangoSearch <br> **Deployment Mode:** Cluster <br> **Description:** ArangoSearch index consolidation does not work during creation of a link on existing collection which may lead to massive file descriptors consumption <br> **Affected Versions:** 3.4.0-RC.5 <br> **Fixed in Versions:** - <br> **Reference:** [arangodb/backlog#509](https://github.com/arangodb/backlog/issues/509) (internal) |
| **Date Added:** 2018-12-03 <br> **Component:** ArangoSearch <br> **Deployment Mode:** Cluster <br> **Description:** Long-running DML transactions on collections (linked with ArangoSearch view) block "ArangoDB flush thread" making impossible to refresh data "visible" by a view <br> **Affected Versions:** 3.4.0-RC.5 <br> **Fixed in Versions:** - <br> **Reference:** [arangodb/backlog#510](https://github.com/arangodb/backlog/issues/510) (internal) |
| **Date Added:** 2018-12-03 <br> **Component:** ArangoSearch <br> **Deployment Mode:** All <br> **Description:** ArangoSearch index format included starting from 3.4.0-RC.4 is incompatible to earlier released 3.4.0 release candidates. Dump and restore is needed when upgrading from 3.4.0-RC.4 to a newer 3.4.0.x release <br> **Affected Versions:** 3.4.0-RC.5 <br> **Fixed in Versions:** - <br> **Reference:** N/A |
| **Date Added:** 2018-12-03 <br> **Component:** ArangoSearch <br> **Deployment Mode:** All <br> **Description:** RocksDB recovery fails sometimes after renaming a view <br> **Affected Versions:** 3.4.0-RC.5 <br> **Fixed in Versions:** - <br> **Reference:** [arangodb/backlog#469](https://github.com/arangodb/backlog/issues/469) (internal) |
| **Date Added:** 2018-12-03 <br> **Component:** ArangoSearch <br> **Deployment Mode:** All <br> **Description:** Using a loop variable in expressions within a corresponding SEARCH condition is not supported <br> **Affected Versions:** 3.4.0-RC.5 <br> **Fixed in Versions:** - <br> **Reference:** [arangodb/backlog#318](https://github.com/arangodb/backlog/issues/318) (internal) |
| **Date Added:** 2018-12-03 <br> **Component:** ArangoSearch <br> **Deployment Mode:** All <br> **Description:** Using score functions (BM25/TFIDF) in ArangoDB expression is not supported <br> **Affected Versions:** 3.4.0-RC.5 <br> **Fixed in Versions:** - <br> **Reference:** [arangodb/backlog#316](https://github.com/arangodb/backlog/issues/316) (internal) |
| **Date Added:** 2018-12-03 <br> **Component:** ArangoSearch <br> **Deployment Mode:** All <br> **Description:** ArangoSearch index format included starting from 3.4.0-RC.3 is incompatible to earlier released 3.4.0 release candidates. Dump and restore is needed when upgrading from 3.4.0-RC.2 to a newer 3.4.0.x release <br> **Affected Versions:** 3.4.0-RC.5 <br> **Fixed in Versions:** - <br> **Reference:** N/A |
