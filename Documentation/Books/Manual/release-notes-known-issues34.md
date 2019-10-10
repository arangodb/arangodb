---
layout: default
description: Important issues affecting the 3.4.x versions of the ArangoDB suite of products
title: ArangoDB v3.4 Known Issues
---
Known Issues in ArangoDB 3.4
============================

This page lists important issues affecting the 3.4.x versions of the ArangoDB
suite of products. It is not a list of all open issues.

Critical issues (ArangoDB Technical & Security Alerts) are also found at
[arangodb.com/alerts](https://www.arangodb.com/alerts/){:target="_blank"}.

ArangoSearch
------------

| Issue      |
|------------|
| **Date Added:** 2018-12-03 <br> **Component:** ArangoSearch <br> **Deployment Mode:** Cluster <br> **Description:** Score values evaluated by corresponding score functions (BM25/TFIDF) may differ in single-server and cluster with a collection having more than 1 shard <br> **Affected Versions:** 3.4.x, 3.5.x <br> **Fixed in Versions:** - <br> **Reference:** [arangodb/backlog#508](https://github.com/arangodb/backlog/issues/508){:target="_blank"} (internal) |
| **Date Added:** 2018-12-03 <br> **Component:** ArangoSearch <br> **Deployment Mode:** Cluster <br> **Description:** ArangoSearch index consolidation does not work during creation of a link on existing collection which may lead to massive file descriptors consumption <br> **Affected Versions:** 3.4.x, 3.5.x <br> **Fixed in Versions:** - <br> **Reference:** [arangodb/backlog#509](https://github.com/arangodb/backlog/issues/509){:target="_blank"} (internal) |
| **Date Added:** 2018-12-03 <br> **Component:** ArangoSearch <br> **Deployment Mode:** Cluster <br> **Description:** Long-running DML transactions on collections (linked with ArangoSearch view) block "ArangoDB flush thread" making impossible to refresh data "visible" by a view <br> **Affected Versions:** 3.4.x <br> **Fixed in Versions:** 3.5.0 <br> **Reference:** [arangodb/backlog#510](https://github.com/arangodb/backlog/issues/510){:target="_blank"} (internal) |
| **Date Added:** 2018-12-03 <br> **Component:** ArangoSearch <br> **Deployment Mode:** All <br> **Description:** RocksDB recovery fails sometimes after renaming a view <br> **Affected Versions:** 3.4.x <br> **Fixed in Versions:** 3.5.0 <br> **Reference:** [arangodb/backlog#469](https://github.com/arangodb/backlog/issues/469){:target="_blank"} (internal) |
| **Date Added:** 2018-12-03 <br> **Component:** ArangoSearch <br> **Deployment Mode:** All <br> **Description:** Using a loop variable in expressions within a corresponding SEARCH condition is not supported <br> **Affected Versions:** 3.4.x, 3.5.x <br> **Fixed in Versions:** - <br> **Reference:** [arangodb/backlog#318](https://github.com/arangodb/backlog/issues/318){:target="_blank"} (internal) |
| **Date Added:** 2018-12-03 <br> **Component:** ArangoSearch <br> **Deployment Mode:** All <br> **Description:** Using score functions (BM25/TFIDF) in ArangoDB expression is not supported <br> **Affected Versions:** 3.4.x <br> **Fixed in Versions:** 3.5.0 <br> **Reference:** [arangodb/backlog#316](https://github.com/arangodb/backlog/issues/316){:target="_blank"} (internal) |
| **Date Added:** 2019-05-16 <br> **Component:** ArangoSearch <br> **Deployment Mode:** All <br> **Description:** You may experience server slowdowns caused by Views. Removing a collection link from the view configuration mitigates the problem. <br> **Affected Versions:** 3.4.x, 3.5.x <br> **Fixed in Versions:** - <br> **Reference:** N/A |
| **Date Added:** 2019-05-28 <br> **Component:** ArangoSearch <br> **Deployment Mode:** All <br> **Description:** For specific input data indexed by ArangoSearch and used via views functionality a crash can occur during lookup (AQL query execution) in the internal index structure. This issue affects only view-related AQL queries with the SEARCH keyword involved. Please upgrade to ArangoDB v3.4.6 (or newer) which contains a fix for the ArangoSearch implementation and reindex views by recreating them (drop and create with the same parameters). <br> **Affected Versions:** 3.4.x <br> **Fixed in Versions:** 3.4.6 <br> **Reference:** N/A |

AQL
---

| Issue      |
|------------|
| **Date Added:** 2018-09-05 <br> **Component:** AQL <br> **Deployment Mode:** Cluster <br> **Description:** In a very uncommon edge case there is an issue with an optimization rule in the cluster. If you are running a cluster and use a custom shard key on a collection (default is `_key`) **and** you provide a wrong shard key in a modifying query (`UPDATE`, `REPLACE`, `DELETE`) **and** the wrong shard key is on a different shard than the correct one, a `DOCUMENT NOT FOUND` error is returned instead of a modification (example query: `UPDATE { _key: "123", shardKey: "wrongKey"} WITH { foo: "bar" } IN mycollection`). Note that the modification always happens if the rule is switched off, so the suggested  workaround is to [deactivate the optimizing rule](aql/execution-and-performance-optimizer.html#turning-specific-optimizer-rules-off) `restrict-to-single-shard`. <br> **Affected Versions:** 3.4.x, 3.5.x <br> **Fixed in Versions:** - <br> **Reference:** [arangodb/arangodb#6399](https://github.com/arangodb/arangodb/issues/6399){:target="_blank"} |

Upgrading
---------

| **Date Added:** 2019-05-16 <br> **Component:** arangod <br> **Deployment Mode:** All <br> **Description:** Upgrading from 3.4.4 to 3.4.x under CentOS using `rpm -U` or `yum upgrade` is unable to succeed because of a problem with the package. The arangodb3 service will not be available anymore. Uninstalling with `rpm -e` or `yum remove` may fail or not clean up all of the folders. <br> **Affected Versions:** >= 3.4.4 <br> **Fixed in Versions:** 3.4.8, 3.5.0 (upgrading works from these versions on again) <br> **Reference:** [arangodb/release-qa#50](https://github.com/arangodb/release-qa/issues/50){:target="_blank"} (internal) |
| **Date Added:** 2019-05-16 <br> **Component:** arangod <br> **Deployment Mode:** All <br> **Description:** Major upgrades such as 3.3.22 to 3.4.5 fail to upgrade the database directory under CentOS. The server log suggests to use the script `/etc/init.d/arangodb` but it is not available under CentOS. Workaround: Add `database.auto-upgrade = true` to `/etc/arangodb3/arangod.conf`, restart the service, remove `database.auto-upgrade = true` from the configuration and restart the service once more. <br> **Affected Versions:** 3.x.x (CentOS only) <br> **Fixed in Versions:** - <br> **Reference:** [arangodb/release-qa#50](https://github.com/arangodb/release-qa/issues/50){:target="_blank"} (internal) |
| **Date Added:** 2019-05-16 <br> **Component:** arangod <br> **Deployment Mode:** All <br> **Description:** Bugfix release upgrades such as 3.4.4 to 3.4.5 may not create a backup of the database directory even if they should. Please create a copy manually before upgrading. <br> **Affected Versions:** 3.4.x, 3.5.x (Windows and Linux) <br> **Fixed in Versions:** - <br> **Reference:** [arangodb/planning#3745](https://github.com/arangodb/planning/issues/3745){:target="_blank"} (internal) |

Other
-----

| Issue      |
|------------|
| **Date Added:** 2018-12-04 <br> **Component:** arangod <br> **Deployment Mode:** All <br> **Description:** Parallel creation of collections using multiple client connections with the same database user may spuriously fail with "Could not update user due to conflict" warnings when setting user permissions on the new collections. A follow-up effect of this may be that access to the just-created collection is denied. <br> **Affected Versions:** 3.4.0 <br> **Fixed in Versions:** 3.4.1 <br> **Reference:** [arangodb/arangodb#5342](https://github.com/arangodb/arangodb/issues/5342){:target="_blank"}  |
| **Date Added:** 2019-02-18 <br> **Component:** arangod <br> **Deployment Mode:** All <br> **Description:** There is a clock overflow bug within Facebook's RocksDB storage engine for Windows. The problem manifests under heavy write loads, including long imports. The Windows server will suddenly block all writes for minutes or hours, then begin working again just fine. An immediate workaround is to change the server configuration: <br>`[rocksdb]`<br>`throttle = false` <br> **Affected Versions:** 3.x.x (Windows only) <br> **Fixed in Versions:** 3.3.23, 3.4.4 <br> **Reference:** [facebook/rocksdb#4983](https://github.com/facebook/rocksdb/issues/4983){:target="_blank"} |
| **Date Added:** 2019-03-13 <br> **Component:** arangod <br> **Deployment Mode:** Active Failover <br> **Description:** A full resync is triggered after a failover, when the former leader instance is brought back online. A full resync may even occur twice sporadically.  <br> **Affected Versions:** all 3.4.x versions <br> **Fixed in Versions:** 3.4.5 <br> **Reference:** [arangodb/planning#3757](https://github.com/arangodb/planning/issues/3757){:target="_blank"} (internal) |
| **Date Added:** 2019-03-13 <br> **Component:** arangod <br> **Deployment Mode:** Active Failover <br> **Description:** The leader instance may hang on shutdown. This behavior was observed in an otherwise successful failover. <br> **Affected Versions:** all 3.4.x versions <br> **Fixed in Versions:** - <br> **Reference:** [arangodb/planning#3756](https://github.com/arangodb/planning/issues/3756){:target="_blank"} (internal) |
| **Date Added:** 2019-05-16 <br> **Component:** Starter <br> **Deployment Mode:** All <br> **Description:** The ArangoDB Starter falls back to the IP `[::1]` under macOS. If there is no entry `::1  localhost` in the `/etc/hosts` file or the option `--starter.disable-ipv6` is passed to the starter to use IPv4, then it will hang during startup. <br> **Affected Versions:** 0.14.3 (macOS only) <br> **Fixed in Versions:** - <br> **Reference:** N/A |
| **Date Added:** 2019-05-29 <br> **Component:** Web UI <br> **Deployment Mode:** All <br> **Description:** The LOGS menu entry in the web interface does not work. <br> **Affected Versions:** 3.4.6 <br> **Fixed in Versions:** 3.4.7 <br> **Reference:** [arangodb/arangodb#9093](https://github.com/arangodb/arangodb/pull/9093){:target="_blank"} |
| **Date Added:** 2019-06-04 <br> **Component:** Agency <br> **Deployment Mode:** Cluster <br> **Description:** Data loss can occur in the form of collections getting dropped. This may occur for any collection newly created in v3.4.6 clusters. <br> **Affected Versions:** 3.4.6 <br> **Fixed in Versions:** 3.4.6-1 <br> **Reference:** [arangodb.com/alerts/tech05/](https://www.arangodb.com/alerts/tech05/){:target="_blank"} |
| **Date Added:** 2019-07-04 <br> **Component:** Config file parsing <br> **Deployment Mode:** All <br> **Description:** Config file parser may produce unexpected values for settings that contain comments and a unit modifier. <br> **Affected Versions:** 3.x.x <br> **Fixed in Versions:** 3.3.24, 3.4.7 <br> **Reference:** [arangodb/arangodb#9404](https://github.com/arangodb/arangodb/issues/9404){:target="_blank"} |
| **Date Added:** 2019-09-14 <br> **Component:** arangod <br> **Deployment Mode:** All <br> **Description:** The user-provided value to the startup option `http.keep-alive-timeout` is ignored. <br> **Affected Versions:** 3.4.x <br> **Fixed in Versions:** 3.5.0 <br> **Reference:** N/A |
