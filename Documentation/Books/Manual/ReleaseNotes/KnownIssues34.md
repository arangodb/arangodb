Known Issues
============

This page lists important issues of the ArangoDB suite of products all users
should take note of. It is not a list of all open issues.

ArangoDB Customers can access the
[_Technical & Security Alerts_](https://arangodb.atlassian.net/wiki/spaces/DEVSUP/pages/223903745)
page after login in the Support Portal.

| # | Issue      |
|---|------------|
| 6 | **Date Added:** 2018-12-03 <br> **Component:** ArangoSearch <br> **Deployment Mode:** All <br> **Description:** RocksDB recovery fails sometimes after renaming a view <br> **Affect Versions:** 3.4.0-RC.5 <br> **Fixed in Versions:** - <br> **Reference:** Internal Issue #469 |
| 5 | **Date Added:** 2018-12-03 <br> **Component:** ArangoSearch <br> **Deployment Mode:** All <br> **Description:** ArangoSearch ignores `_id` attribute even if `includeAllFields` is set to `true`  <br> **Affect Versions:** 3.4.0-RC.5 <br> **Fixed in Versions:** - <br> **Reference:** Internal Issue #445 |
| 4 | **Date Added:** 2018-12-03 <br> **Component:** ArangoSearch <br> **Deployment Mode:** All <br> **Description:** Using a loop variable in expressions within a corresponding SEARCH condition is not supported <br> **Affect Versions:** 3.4.0-RC.5 <br> **Fixed in Versions:** - <br> **Reference:** Internal Issue #318 |
| 3 | **Date Added:** 2018-12-03 <br> **Component:** ArangoSearch <br> **Deployment Mode:** All <br> **Description:** Using score functions (BM25/TFIDF) in ArangoDB expression is not supported <br> **Affect Versions:** 3.4.0-RC.5 <br> **Fixed in Versions:** - <br> **Reference:** Internal Issue #316 |
| 2 | **Date Added:** 2018-12-03 <br> **Component:** ArangoSearch <br> **Deployment Mode:** All <br> **Description:** ArangoSearch index format included starting from 3.4.0-RC.3 is incompatible to earlier released 3.4.0 release candidates. Dump and restore is needed when upgrading from 3.4.0-RC.2 to a newer 3.4.0.x release <br> **Affect Versions:** 3.4.0-RC.5 <br> **Fixed in Versions:** - <br> **Reference:** NA |
| 1 | **Date Added:** 2018-09-05 <br> **Component:** AQL <br> **Deployment Mode:** Cluster <br> **Description:** In a very uncommon edge case there is an issue with an optimization rule in the cluster. If you are running a cluster and use a custom shard key on a collection (default is `_key`) **and** you provide a wrong shard key in a modifying query (`UPDATE`, `REPLACE`, `DELETE`) **and** the wrong shard key is on a different shard than the correct one, a `DOCUMENT NOT FOUND` error is returned instead of a modification (example query: `UPDATE { _key: "123", shardKey: "wrongKey"} WITH { foo: "bar" } IN mycollection`). Note that the modification always happens if the rule is switched off, so the suggested  workaround is to [deactivate the optimizing rule](../../AQL/ExecutionAndPerformance/Optimizer.html#turning-specific-optimizer-rules-off) `restrict-to-single-shard`. <br> **Affect Versions:** 3.4.0-RC.5 <br> **Fixed in Versions:** - <br> **Reference:** [Public Bug Report](https://github.com/arangodb/arangodb/issues/6399) |
