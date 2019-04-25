Known Issues in ArangoDB 3.4
============================

This page lists important issues affecting the 3.4.x versions of the ArangoDB suite of products.
It is not a list of all open issues.

Critical issues (ArangoDB Technical & Security Alerts) are also found at [arangodb.com/alerts](https://www.arangodb.com/alerts/).

ArangoSearch
------------

| Issue      |
|------------|
| **Date Added:** 2018-12-03 <br> **Component:** ArangoSearch <br> **Deployment Mode:** Cluster <br> **Description:** Score values evaluated by corresponding score functions (BM25/TFIDF) may differ in single-server and cluster with a collection having more than 1 shard <br> **Affected Versions:** 3.4.0-RC.5 <br> **Fixed in Versions:** - <br> **Reference:** [arangodb/backlog#508](https://github.com/arangodb/backlog/issues/508) (internal) |
| **Date Added:** 2018-12-03 <br> **Component:** ArangoSearch <br> **Deployment Mode:** Cluster <br> **Description:** ArangoSearch index consolidation does not work during creation of a link on existing collection which may lead to massive file descriptors consumption <br> **Affected Versions:** 3.4.0-RC.5 <br> **Fixed in Versions:** - <br> **Reference:** [arangodb/backlog#509](https://github.com/arangodb/backlog/issues/509) (internal) |
| **Date Added:** 2018-12-03 <br> **Component:** ArangoSearch <br> **Deployment Mode:** Cluster <br> **Description:** Long-running DML transactions on collections (linked with ArangoSearch view) block "ArangoDB flush thread" making impossible to refresh data "visible" by a view <br> **Affected Versions:** 3.4.0-RC.5 <br> **Fixed in Versions:** - <br> **Reference:** [arangodb/backlog#510](https://github.com/arangodb/backlog/issues/510) (internal) |
| **Date Added:** 2018-12-03 <br> **Component:** ArangoSearch <br> **Deployment Mode:** All <br> **Description:** ArangoSearch index format included starting from 3.4.0-RC.4 is incompatible to earlier released 3.4.0 release candidates. Dump and restore is needed when upgrading from 3.4.0-RC.4 to a newer 3.4.0.x release <br> **Affected Versions:** 3.4.0-RC.5 <br> **Fixed in Versions:** - <br> **Reference:** N/A |
| **Date Added:** 2018-12-03 <br> **Component:** ArangoSearch <br> **Deployment Mode:** All <br> **Description:** RocksDB recovery fails sometimes after renaming a view <br> **Affected Versions:** 3.4.0-RC.5 <br> **Fixed in Versions:** - <br> **Reference:** [arangodb/backlog#469](https://github.com/arangodb/backlog/issues/469) (internal) |
| **Date Added:** 2018-12-03 <br> **Component:** ArangoSearch <br> **Deployment Mode:** All <br> **Description:** ArangoSearch ignores `_id` attribute even if `includeAllFields` is set to `true`  <br> **Affected Versions:** 3.4.0-RC.5 <br> **Fixed in Versions:** - <br> **Reference:** [arangodb/backlog#445](https://github.com/arangodb/backlog/issues/445) (internal) |
| **Date Added:** 2018-12-03 <br> **Component:** ArangoSearch <br> **Deployment Mode:** All <br> **Description:** Using a loop variable in expressions within a corresponding SEARCH condition is not supported <br> **Affected Versions:** 3.4.0-RC.5 <br> **Fixed in Versions:** - <br> **Reference:** [arangodb/backlog#318](https://github.com/arangodb/backlog/issues/318) (internal) |
| **Date Added:** 2018-12-03 <br> **Component:** ArangoSearch <br> **Deployment Mode:** All <br> **Description:** Using score functions (BM25/TFIDF) in ArangoDB expression is not supported <br> **Affected Versions:** 3.4.0-RC.5 <br> **Fixed in Versions:** - <br> **Reference:** [arangodb/backlog#316](https://github.com/arangodb/backlog/issues/316) (internal) |
| **Date Added:** 2018-12-03 <br> **Component:** ArangoSearch <br> **Deployment Mode:** All <br> **Description:** ArangoSearch index format included starting from 3.4.0-RC.3 is incompatible to earlier released 3.4.0 release candidates. Dump and restore is needed when upgrading from 3.4.0-RC.2 to a newer 3.4.0.x release <br> **Affected Versions:** 3.4.0-RC.5 <br> **Fixed in Versions:** - <br> **Reference:** N/A |


AQL
---

| Issue      |
|------------|
| **Date Added:** 2018-09-05 <br> **Component:** AQL <br> **Deployment Mode:** Cluster <br> **Description:** In a very uncommon edge case there is an issue with an optimization rule in the cluster. If you are running a cluster and use a custom shard key on a collection (default is `_key`) **and** you provide a wrong shard key in a modifying query (`UPDATE`, `REPLACE`, `DELETE`) **and** the wrong shard key is on a different shard than the correct one, a `DOCUMENT NOT FOUND` error is returned instead of a modification (example query: `UPDATE { _key: "123", shardKey: "wrongKey"} WITH { foo: "bar" } IN mycollection`). Note that the modification always happens if the rule is switched off, so the suggested  workaround is to [deactivate the optimizing rule](../../AQL/ExecutionAndPerformance/Optimizer.html#turning-specific-optimizer-rules-off) `restrict-to-single-shard`. <br> **Affected Versions:** 3.4.0-RC.5 <br> **Fixed in Versions:** - <br> **Reference:** [arangodb/arangodb#6399](https://github.com/arangodb/arangodb/issues/6399) |


Other
-----

| Issue      |
|------------|
| **Date Added:** 2018-12-04 <br> **Component:** arangod <br> **Deployment Mode:** All <br> **Description:** Parallel creation of collections using multiple client connections with the same database user may spuriously fail with "Could not update user due to conflict" warnings when setting user permissions on the new collections. A follow-up effect of this may be that access to the just-created collection is denied. <br> **Affected Versions:** 3.4.0 <br> **Fixed in Versions:** 3.4.1 <br> **Reference:** [arangodb/arangodb#5342](https://github.com/arangodb/arangodb/issues/5342)  |
| **Date Added:** 2019-02-18 <br> **Component:** arangod <br> **Deployment Mode:** All <br> **Description:** There is a clock overflow bug within Facebook's RocksDB storage engine for Windows. The problem manifests under heavy write loads, including long imports. The Windows server will suddenly block all writes for minutes or hours, then begin working again just fine. An immediate workaround is to change the server configuration: <pre><code>[rocksdb]&#13;throttle = false</code></pre> **Affected Versions:** all 3.x versions (Windows only) <br> **Fixed in Versions:** 3.3.23, 3.4.4 <br> **Reference:** [facebook/rocksdb#4983](https://github.com/facebook/rocksdb/issues/4983) |
| **Date Added:** 2019-03-13 <br> **Component:** arangod <br> **Deployment Mode:** Active Failover <br> **Description:** A full resync is triggered after a failover, when the former leader instance is brought back online. A full resync may even occur twice sporadically.  <br> **Affected Versions:** all 3.4.x versions <br> **Fixed in Versions:** 3.4.5 <br> **Reference:** [arangodb/planning#3757](https://github.com/arangodb/planning/issues/3757) (internal) |
| **Date Added:** 2019-03-13 <br> **Component:** arangod <br> **Deployment Mode:** Active Failover <br> **Description:** The leader instance may hang on shutdown. This behavior was observed in an otherwise successful failover. <br> **Affected Versions:** all 3.4.x versions <br> **Fixed in Versions:** - <br> **Reference:** [arangodb/planning#3756](https://github.com/arangodb/planning/issues/3756) (internal) |
