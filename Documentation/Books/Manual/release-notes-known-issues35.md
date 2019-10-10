---
layout: default
description: Important issues affecting the 3.5.x versions of the ArangoDB suite of products
title: ArangoDB v3.5 Known Issues
---
Known Issues in ArangoDB 3.5
============================

This page lists important issues affecting the 3.5.x versions of the ArangoDB
suite of products. It is not a list of all open issues.

Critical issues (ArangoDB Technical & Security Alerts) are also found at
[arangodb.com/alerts](https://www.arangodb.com/alerts/){:target="_blank"}.

ArangoSearch
------------

| Issue      |
|------------|
| **Date Added:** 2018-12-19 <br> **Component:** ArangoSearch <br> **Deployment Mode:** Single-server <br> **Description:** Value of `_id` attribute indexed by ArangoSearch view may become inconsistent after renaming a collection <br> **Affected Versions:** 3.5.x <br> **Fixed in Versions:** - <br> **Reference:** [arangodb/backlog#514](https://github.com/arangodb/backlog/issues/514){:target="_blank"} (internal) |
| **Date Added:** 2018-12-03 <br> **Component:** ArangoSearch <br> **Deployment Mode:** Cluster <br> **Description:** Score values evaluated by corresponding score functions (BM25/TFIDF) may differ in single-server and cluster with a collection having more than 1 shard <br> **Affected Versions:** 3.4.x, 3.5.x <br> **Fixed in Versions:** - <br> **Reference:** [arangodb/backlog#508](https://github.com/arangodb/backlog/issues/508){:target="_blank"} (internal) |
| **Date Added:** 2018-12-03 <br> **Component:** ArangoSearch <br> **Deployment Mode:** Cluster <br> **Description:** ArangoSearch index consolidation does not work during creation of a link on existing collection which may lead to massive file descriptors consumption <br> **Affected Versions:** 3.4.x, 3.5.x <br> **Fixed in Versions:** - <br> **Reference:** [arangodb/backlog#509](https://github.com/arangodb/backlog/issues/509){:target="_blank"} (internal) |
| **Date Added:** 2018-12-03 <br> **Component:** ArangoSearch <br> **Deployment Mode:** All <br> **Description:** Using a loop variable in expressions within a corresponding SEARCH condition is not supported <br> **Affected Versions:** 3.4.x, 3.5.x <br> **Fixed in Versions:** - <br> **Reference:** [arangodb/backlog#318](https://github.com/arangodb/backlog/issues/318){:target="_blank"} (internal) |
| **Date Added:** 2019-06-25 <br> **Component:** ArangoSearch <br> **Deployment Mode:** All <br> **Description:** The `primarySort` attribute in ArangoSearch View definitions can not be set via the web interface. The option is immutable, but the web interface does not allow to set any View properties upfront (it creates a View with default parameters before the user has a chance to configure it). <br> **Affected Versions:** 3.5.x <br> **Fixed in Versions:** - <br> **Reference:** N/A |

AQL
---

| Issue      |
|------------|
| **Date Added:** 2018-09-05 <br> **Component:** AQL <br> **Deployment Mode:** Cluster <br> **Description:** In a very uncommon edge case there is an issue with an optimization rule in the cluster. If you are running a cluster and use a custom shard key on a collection (default is `_key`) **and** you provide a wrong shard key in a modifying query (`UPDATE`, `REPLACE`, `DELETE`) **and** the wrong shard key is on a different shard than the correct one, a `DOCUMENT NOT FOUND` error is returned instead of a modification (example query: `UPDATE { _key: "123", shardKey: "wrongKey"} WITH { foo: "bar" } IN mycollection`). Note that the modification always happens if the rule is switched off, so the suggested  workaround is to [deactivate the optimizing rule](aql/execution-and-performance-optimizer.html#turning-specific-optimizer-rules-off) `restrict-to-single-shard`. <br> **Affected Versions:** 3.4.x, 3.5.x <br> **Fixed in Versions:** - <br> **Reference:** [arangodb/arangodb#6399](https://github.com/arangodb/arangodb/issues/6399){:target="_blank"} |

Upgrading
---------

| Issue      |
|------------|
| **Date Added:** 2019-05-16 <br> **Component:** arangod <br> **Deployment Mode:** All <br> **Description:** Major upgrades such as 3.3.22 to 3.4.5 fail to upgrade the database directory under CentOS. The server log suggests to use the script `/etc/init.d/arangodb` but it is not available under CentOS. Workaround: Add `database.auto-upgrade = true` to `/etc/arangodb3/arangod.conf`, restart the service, remove `database.auto-upgrade = true` from the configuration and restart the service once more. <br> **Affected Versions:** 3.x.x (CentOS only) <br> **Fixed in Versions:** - <br> **Reference:** [arangodb/release-qa#50](https://github.com/arangodb/release-qa/issues/50){:target="_blank"} (internal) |
| **Date Added:** 2019-05-16 <br> **Component:** arangod <br> **Deployment Mode:** All <br> **Description:** Bugfix release upgrades such as 3.4.4 to 3.4.5 may not create a backup of the database directory even if they should. Please create a copy manually before upgrading. <br> **Affected Versions:** 3.4.x, 3.5.x (Windows and Linux) <br> **Fixed in Versions:** - <br> **Reference:** [arangodb/planning#3745](https://github.com/arangodb/planning/issues/3745){:target="_blank"} (internal) |

Stream Transactions
-------------------

| Issue      |
|------------|
| **Date Added:** 2019-08-19 <br> **Component:** Transactions <br> **Deployment Mode:** All <br> **Description:** Stream Transactions do not honor the limits described in the documentation. Currently the idle timeout of 10 seconds will not be enforced, neither will the maximum size of transaction be enforced. <br> **Affected Versions:** 3.5.0 <br> **Fixed in Versions:** 3.5.1 <br> **Reference:** [arangodb/arangodb#9775](https://github.com/arangodb/arangodb/pull/9775){:target="_blank"} |
| **Date Added:** 2019-08-19 <br> **Component:** Transactions <br> **Deployment Mode:** All <br> **Description:** Stream Transactions do not support the graph operations that are initiated via the `general-graph` / `smart-graph` JavaScript module or via the REST API at `/_api/gharial`. These operations will act as if no stream transaction is present. <br> **Affected Versions:** 3.5.0 <br> **Fixed in Versions:** 3.5.1 <br> **Reference:** [arangodb/arangodb#9855](https://github.com/arangodb/arangodb/pull/9855){:target="_blank"} / [arangodb/arangodb#9911](https://github.com/arangodb/arangodb/pull/9911){:target="_blank"} |
| **Date Added:** 2019-08-19 <br> **Component:** Transactions <br> **Deployment Mode:** All <br> **Description:** Stream Transactions do not support user restrictions. Any authenticated user may access any ongoing transaction so long as they have access to the database in question. <br> **Affected Versions:** 3.5.0 <br> **Fixed in Versions:** 3.5.1 <br> **Reference:** [arangodb/arangodb#9796](https://github.com/arangodb/arangodb/pull/9796){:target="_blank"} |

Other
-----

| Issue      |
|------------|
| **Date Added:** 2019-05-16 <br> **Component:** Starter <br> **Deployment Mode:** All <br> **Description:** The ArangoDB Starter falls back to the IP `[::1]` under macOS. If there is no entry `::1  localhost` in the `/etc/hosts` file or the option `--starter.disable-ipv6` is passed to the starter to use IPv4, then it will hang during startup. <br> **Affected Versions:** 0.14.3 (macOS only) <br> **Fixed in Versions:** - <br> **Reference:** N/A |
| **Date Added:** 2019-05-16 <br> **Component:** arangod <br> **Deployment Mode:** All <br> **Description:** Calling a shutdown endpoint may not result in a proper shutdown while the node/server is still under load. The server processes must be ended manually. <br> **Affected Versions:** 3.5.x <br> **Fixed in Versions:** - <br> **Reference:** N/A |
| **Date Added:** 2019-05-24 <br> **Component:** Web UI <br> **Deployment Mode:** Active Failover <br> **Description:** The web interface shows a wrong replication mode in the replication tab in Active Failover deployments sometimes. It may display Master/Slave mode (the default value) because of timeouts if `/_api/cluster/endpoints` is requested too frequently. <br> **Affected Versions:** 3.5.x <br> **Fixed in Versions:** - <br> **Reference:** N/A |
| **Date Added:** 2019-09-11 <br> **Component:** Indexing <br> **Deployment Mode:** All <br> **Description:** A time to live (TTL) index does not remove documents from a collection if the path points to a nested attribute. Only top-level attributes work. <br> **Affected Versions:** 3.5.x <br> **Fixed in Versions:** - <br> **Reference:** N/A |
