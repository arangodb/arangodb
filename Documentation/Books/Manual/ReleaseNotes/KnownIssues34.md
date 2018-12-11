Known Issues in ArangoDB 3.4
============================

This page lists important issues affecting the 3.4.x versions of the ArangoDB suite of products.
It is not a list of all open issues.

ArangoDB Customers can access the
[_Technical & Security Alerts_](https://arangodb.atlassian.net/wiki/spaces/DEVSUP/pages/223903745)
page after login into the Support Portal.

ArangoSearch
------------

| # | Issue      |
|---|------------|
| 9 | **Date Added:** 2018-12-03 <br> **Component:** ArangoSearch <br> **Deployment Mode:** Cluster <br> **Description:** Score values evaluated by corresponding score functions (BM25/TFIDF) may differ in single-server and cluster with a collection having more than 1 shard <br> **Affected Versions:** 3.4.0-RC.5 <br> **Fixed in Versions:** - <br> **Reference:** [arangodb/backlog#508](https://github.com/arangodb/backlog/issues/508) (internal) |
| 8 | **Date Added:** 2018-12-03 <br> **Component:** ArangoSearch <br> **Deployment Mode:** Cluster <br> **Description:** ArangoSearch index consolidation does not work during creation of a link on existing collection which may lead to massive file descriptors consumption <br> **Affected Versions:** 3.4.0-RC.5 <br> **Fixed in Versions:** - <br> **Reference:** [arangodb/backlog#509](https://github.com/arangodb/backlog/issues/509) (internal) |
| 7 | **Date Added:** 2018-12-03 <br> **Component:** ArangoSearch <br> **Deployment Mode:** Cluster <br> **Description:** Long-running DML transactions on collections (linked with ArangoSearch view) block "ArangoDB flush thread" making impossible to refresh data "visible" by a view <br> **Affected Versions:** 3.4.0-RC.5 <br> **Fixed in Versions:** - <br> **Reference:** [arangodb/backlog#510](https://github.com/arangodb/backlog/issues/510) (internal) |
| 6 | **Date Added:** 2018-12-03 <br> **Component:** ArangoSearch <br> **Deployment Mode:** All <br> **Description:** ArangoSearch index format included starting from 3.4.0-RC.4 is incompatible to earlier released 3.4.0 release candidates. Dump and restore is needed when upgrading from 3.4.0-RC.4 to a newer 3.4.0.x release <br> **Affected Versions:** 3.4.0-RC.5 <br> **Fixed in Versions:** - <br> **Reference:** N/A |
| 5 | **Date Added:** 2018-12-03 <br> **Component:** ArangoSearch <br> **Deployment Mode:** All <br> **Description:** RocksDB recovery fails sometimes after renaming a view <br> **Affected Versions:** 3.4.0-RC.5 <br> **Fixed in Versions:** - <br> **Reference:** [arangodb/backlog#469](https://github.com/arangodb/backlog/issues/469) (internal) |
| 4 | **Date Added:** 2018-12-03 <br> **Component:** ArangoSearch <br> **Deployment Mode:** All <br> **Description:** ArangoSearch ignores `_id` attribute even if `includeAllFields` is set to `true`  <br> **Affected Versions:** 3.4.0-RC.5 <br> **Fixed in Versions:** - <br> **Reference:** [arangodb/backlog#445](https://github.com/arangodb/backlog/issues/445) (internal) |
| 3 | **Date Added:** 2018-12-03 <br> **Component:** ArangoSearch <br> **Deployment Mode:** All <br> **Description:** Using a loop variable in expressions within a corresponding SEARCH condition is not supported <br> **Affected Versions:** 3.4.0-RC.5 <br> **Fixed in Versions:** - <br> **Reference:** [arangodb/backlog#318](https://github.com/arangodb/backlog/issues/318) (internal) |
| 2 | **Date Added:** 2018-12-03 <br> **Component:** ArangoSearch <br> **Deployment Mode:** All <br> **Description:** Using score functions (BM25/TFIDF) in ArangoDB expression is not supported <br> **Affected Versions:** 3.4.0-RC.5 <br> **Fixed in Versions:** - <br> **Reference:** [arangodb/backlog#316](https://github.com/arangodb/backlog/issues/316) (internal) |
| 1 | **Date Added:** 2018-12-03 <br> **Component:** ArangoSearch <br> **Deployment Mode:** All <br> **Description:** ArangoSearch index format included starting from 3.4.0-RC.3 is incompatible to earlier released 3.4.0 release candidates. Dump and restore is needed when upgrading from 3.4.0-RC.2 to a newer 3.4.0.x release <br> **Affected Versions:** 3.4.0-RC.5 <br> **Fixed in Versions:** - <br> **Reference:** N/A |


AQL
---

| # | Issue      |
|---|------------|
| 1 | **Date Added:** 2018-09-05 <br> **Component:** AQL <br> **Deployment Mode:** Cluster <br> **Description:** In a very uncommon edge case there is an issue with an optimization rule in the cluster. If you are running a cluster and use a custom shard key on a collection (default is `_key`) **and** you provide a wrong shard key in a modifying query (`UPDATE`, `REPLACE`, `DELETE`) **and** the wrong shard key is on a different shard than the correct one, a `DOCUMENT NOT FOUND` error is returned instead of a modification (example query: `UPDATE { _key: "123", shardKey: "wrongKey"} WITH { foo: "bar" } IN mycollection`). Note that the modification always happens if the rule is switched off, so the suggested  workaround is to [deactivate the optimizing rule](../../AQL/ExecutionAndPerformance/Optimizer.html#turning-specific-optimizer-rules-off) `restrict-to-single-shard`. <br> **Affected Versions:** 3.4.0-RC.5 <br> **Fixed in Versions:** - <br> **Reference:** [arangodb/arangodb#6399](https://github.com/arangodb/arangodb/issues/6399) |


Other
---

| # | Issue      |
|---|------------|
| 1 | **Date Added:** 2018-12-04 <br> **Component:** arangod <br> **Deployment Mode:** All <br> **Description:** Parallel creation of collections using multiple client connections with the same database user may spuriously fail with "Could not update user due to conflict" warnings when setting user permissions on the new collections. A follow-up effect of this may be that access to the just-created collection is denied. <br> **Affected Versions:** 3.4.0 <br> **Fixed in Versions:** 3.4.1 <br> **Reference:** [arangodb/arangodb#5342](https://github.com/arangodb/arangodb/issues/5342)  |
