---
layout: default
description: All Editions
---
Highlights
==========

Version 3.5
-----------

**All Editions**

- **ArangoSearch**:
  The search and ranking engine received an upgrade and now features
  [Configurable Analyzers](arangosearch-analyzers.html),
  [Sorted Views](arangosearch-views.html#primary-sort-order)
  and several improvements to the
  [AQL integration](release-notes-new-features35.html#arangosearch).

- **AQL Graph Traversals**:
  [k Shortest Paths](aql/graphs-kshortest-paths.html) allows you to query not
  just for one shortest path between two documents but multiple, sorted by
  length or weight. With [PRUNE](aql/graphs-traversals.html#pruning) you can
  stop walking down certain paths early in a graph traversal to improve its
  efficiency.

- [**Stream Transaction API**](http/transaction-stream-transaction.html):
  Perform multi-document transactions with individual begin and commit / abort
  commands using the new HTTP endpoints or via a supported driver.

- [**Time-to-Live**](indexing-index-basics.html#ttl-time-to-live-index)
  [**Indexes**](indexing-ttl.html):
  TTL indexes can be used to automatically remove documents in collections for
  use cases like expiring sessions or automatic purging of statistics or logs.

- [**Index Hints**](aql/operations-for.html#index-hints) &
  [**Named Indexes**](https://www.arangodb.com/arangodb-training-center/index-hints-named-indices/){:target="_blank"}
  Indexes can be given names and an optional AQL inline query option
  `indexHint` was added to override the internal optimizer decision on which
  index to utilize.

- [**Data Masking**](programs-arangodump-maskings.html):
  arangodump provides a convenient way to extract production data but mask
  critical information that should not be visible.

**Enterprise Edition**

- [**SmartJoins**](smartjoins.html):
  SmartJoins allow to run joins between identically sharded collections with
  performance close to that of a local join operation.

- **Advanced Data Masking**:
  There are additional
  [data masking functions](programs-arangodump-maskings.html#masking-functions)
  available in the Enterprise Edition, such as to substitute email addresses,
  phone numbers etc. with similar looking pseudo-data.

Version 3.4
-----------

**All Editions**

- [**ArangoSearch**](arangosearch.html):
  Search and similarity ranking engine integrated natively into ArangoDB and
  AQL. ArangoSearch combines Boolean retrieval capabilities with generalized
  ranking algorithms (BM25, TFDIF). Support of e.g. relevance-based searching,
  phrase and prefix-matching, complex boolean searches and query time relevance
  tuning. Search can be combined with all supported data models in a single
  query. Many specialized language analyzers are already included for e.g.
  English, German, French, Chinese, Spanish and many other language.

- [**GeoJSON Support**](aql/functions-geo.html) and
  [**S2 Geo Index**](indexing-geo.html): ArangoDB now supports all geo primitives.
  (Multi-)Point, (Multi-)LineStrings, (Multi-)Polygons or intersections can be
  defined and queried for. The Google S2 geo index is optimized for RocksDB and
  enables efficient querying. Geo query results are automatically visualized
  with an OpenStreetMap integration within the Query Editor of the web interface.

- [**Query Profiler**](aql/execution-and-performance-query-profiler.html):
  Enables the analysis of queries and adds additional information for the user
  to identify optimization potentials more easily. The profiler can be accessed
  via Arangosh with `db._profileQuery(...)` or via the *Profile* button in the
  Query Editor of the web interface.

- [**Streaming Cursors**](aql/invocation-with-arangosh.html#setting-options):
  Cursors requested with the stream option on make queries calculate results
  on the fly and make them available for the client in a streaming fashion,
  as soon as possible.

- **RocksDB as Default Storage Engine**: With ArangoDB 3.4 the default
  [storage engine](architecture-storage-engines.html) for fresh installations will
  switch from MMFiles to RocksDB. Many optimizations have been made to RocksDB
  since the first release in 3.2. For 3.4 we optimized the binary storage
  format for improved insertion, implemented "optional caching", reduced the
  replication catch-up time and much more.

Also see [What's New in 3.4](release-notes-new-features34.html).

Version 3.3
-----------

**Enterprise Edition**

- [**Datacenter to Datacenter Replication**](deployment-dc2-dc.html):
  Replicate the entire structure and content of an ArangoDB cluster
  asynchronously to another cluster in a different datacenter with ArangoSync.
  Multi-datacenter support means you can fallback to a replica of your cluster
  in case of a disaster in one datacenter.

- [**Encrypted Backups**](programs-arangodump-examples.html#encryption):
  Arangodump can create backups encrypted with a secret key using AES256
  block cipher.

**All Editions**

- [**Server-level Replication**](administration-master-slave-server-level-setup.html):
  In addition to per-database replication, there is now an additional
  `globalApplier`. Start the global replication on the slave once and all
  current and future databases will be replicated from the master to the
  slave automatically.

- [**Asynchronous Failover**](release-notes-new-features33.html#asynchronous-failover):
  Make a single server instance resilient with a second server instance, one
  as master and the other as asynchronously replicating slave, with automatic
  failover to the slave if the master goes down.

Also see [What's New in 3.3](release-notes-new-features33.html).

Version 3.2
-----------

**All Editions**

- [**RocksDB Storage Engine**](architecture-storage-engines.html): You can now use
  as much data in ArangoDB as you can fit on your disk. Plus, you can enjoy
  performance boosts on writes by having only document-level locks.

- [**Pregel**](graphs-pregel.html):
  We implemented distributed graph processing with Pregel to discover hidden
  patterns, identify communities and perform in-depth analytics of large graph
  data sets.

- [**Fault-Tolerant Foxx**](http/foxx.html): The Foxx management
  internals have been rewritten from the ground up to make sure
  multi-coordinator cluster setups always keep their services in sync and
  new coordinators are fully initialized even when all existing coordinators
  are unavailable.

**Enterprise Edition**

- [**LDAP integration**](programs-arangod-ldap.html): Users and permissions
  can be managed from outside ArangoDB with an LDAP server in different
  authentication configurations.

- [**Encryption at Rest**](security-encryption.html): Let the server
  persist your sensitive data strongly encrypted to protect it even if the
  physical storage medium gets stolen.

- [**Satellite Collections**](satellites.html): Faster join operations when
  working with sharded datasets by synchronously replicating selected
  collections to all database servers in a cluster, so that joins can be
  executed locally.

Also see [What's New in 3.2](release-notes-new-features32.html).

Version 3.1
-----------

**All Editions**

- [**Vertex-centric indices**](indexing-vertex-centric.html):
  AQL traversal queries can utilize secondary edge collection
  indexes for better performance against graphs with supernodes.

- [**VelocyPack over HTTP**](https://www.arangodb.com/2016/10/updated-java-drivers-with-arangodb-3-1/){:target="_blank"}:
  In addition to JSON, the binary storage format VelocyPack can now also be
  used in transport over the HTTP protocol, as well as streamed using the new
  bi-directional asynchronous binary protocol **VelocyStream**.

**Enterprise Edition**

- [**SmartGraphs**](graphs-smart-graphs.html): Scale with graphs to a
  cluster and stay performant. With SmartGraphs you can use the "smartness"
  of your application layer to shard your graph efficiently to your machines
  and let traversals run locally.

- **Encryption Control**: Choose your level of [SSL encryption](programs-arangod-ssl.html)

- [**Auditing**](security-auditing.html): Keep a detailed log
  of all the important things that happened in ArangoDB.

Also see [What's New in 3.1](release-notes-new-features31.html).

Version 3.0
-----------

- [**self-organizing cluster**](architecture-deployment-modes-cluster-architecture.html) with
  synchronous replication, master/master setup, shared nothing
  architecture, cluster management agency.

- Deeply integrated, native [**AQL graph traversal**](aql/graphs.html)

- [**VelocyPack**](https://github.com/arangodb/velocypack){:target="_blank"} as new internal
  binary storage format as well as for intermediate AQL values.

- [**Persistent indexes**](indexing-persistent.html) via RocksDB suitable
  for sorting and range queries.

- [**Foxx 3.0**](foxx.html): overhauled JS framework for data-centric
  microservices

- Significantly improved [**Web Interface**](programs-web-interface.html)
  
Also see [What's New in 3.0](release-notes-new-features30.html).
