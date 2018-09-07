Highlights
==========

Version 3.4
-----------

**All Editions**

- [**ArangoSearch**](Views/ArangoSearch/README.md):
  Search and similarity ranking engine integrated natively into ArangoDB and
  AQL. ArangoSearch combines boolean retrieval capabilities with generalized
  ranking algorithms (BM25, TFDIF). Support of e.g. relevance-based searching,
  phrase and prefix-matching, complex boolean searches and query time relevance
  tuning. Search can be combined with all supported data models in a single
  query. Many specialized language analyzers are already included for e.g.
  English, German, French, Chinese, Spanish and many other language.

- [**GeoJSON Support**](../AQL/Functions/Geo.html) and
  [**S2 Geo Index**](Indexing/Geo.md): ArangoDB supports now all geo primitives.
  MultiPoint, MultiLineStrings, MultiPolygons or intersections can be now
  defined and queries for. New Google S2 geo index is optimized for RocksDB and
  enables efficient querying. Geo query results are automatically visualized
  with an OpenStreetMap integration within the Query Editor (ArangoDB WebUI)

- [**Query Profiler**](../AQL/ExecutionAndPerformance/QueryProfiler.html):
  Enables the analysis of queries and adds additional information for the user
  to identify optimization potentials more easily. The profiler can be accessed
  via Arangosh with `db._profileQuery(...)` or via the *Profile* button in the
  Query Editor of the Web UI.

- [**Streaming Cursors**](../AQL/Invocation/WithArangosh.html#setting-options):
  Cursors requested with the stream option on make queries calculate results
  on the fly and make them available for the client in a streaming fashion,
  as soon as possible.

- **RocksDB as Default Storage Engine**: With ArangoDB 3.4 the default
  [storage engine](Architecture/StorageEngines.md) for fresh installations will
  switch from MMFiles to RocksDB. Many optimizations have been made to RocksDB
  since the first release in 3.2. For 3.4 we optimized the binary storage
  format for improved insertion, implemented "optional caching", reduced the
  replication catch-up time and much more.

Also see [What's New in 3.4](ReleaseNotes/NewFeatures34.md).

Version 3.3
-----------

**Enterprise Edition**

- [**Datacenter to Datacenter Replication**](Deployment/DC2DC/README.md): Replicate
  the entire structure and content of an ArangoDB cluster asynchronously to
  another cluster in a different datacenter with ArangoSync. Multi-datacenter
  support means you can fallback to a replica of your cluster in case of a
  disaster in one datacenter.

- [**Encrypted Backups**](Programs/Arangodump/Examples.md#encryption):
  Arangodump can create backups encrypted with a secret key using AES256
  block cipher.

**All Editions**

- [**Server-level Replication**](Administration/MasterSlave/ServerLevelSetup.md):
  In addition to per-database replication, there is now an additional
  `globalApplier`. Start the global replication on the slave once and all
  current and future databases will be replicated from the master to the
  slave automatically.

- [**Asynchronous Failover**](ReleaseNotes/NewFeatures33.md#asynchronous-failover):
  Make a single server instance resilient with a second server instance, one
  as master and the other as asynchronously replicating slave, with automatic
  failover to the slave if the master goes down.

Also see [What's New in 3.3](ReleaseNotes/NewFeatures33.md).

Version 3.2
-----------

- [**RocksDB Storage Engine**](Architecture/StorageEngines.md): You can now use
  as much data in ArangoDB as you can fit on your disk. Plus, you can enjoy
  performance boosts on writes by having only document-level locks

- [**Pregel**](Graphs/Pregel/README.md):
  We implemented distributed graph processing with Pregel to discover hidden
  patterns, identify communities and perform in-depth analytics of large graph
  data sets.

- [**Fault-Tolerant Foxx**](../HTTP/Foxx/index.html): The Foxx management
  internals have been rewritten from the ground up to make sure
  multi-coordinator cluster setups always keep their services in sync and
  new coordinators are fully initialized even when all existing coordinators
  are unavailable.

- **Enterprise**: Working with some of our largest customers, we have added
  further security and scalability features to ArangoDB Enterprise like
  [LDAP integration](Programs/Arangod/Ldap.md),
  [Encryption at Rest](Security/Encryption/README.md), and the brand new
  [Satellite Collections](Satellites.md).

Also see [What's New in 3.2](ReleaseNotes/NewFeatures32.md).

Version 3.1
-----------

- [**SmartGraphs**](Graphs/SmartGraphs/README.md): Scale with graphs to a
  cluster and stay performant. With SmartGraphs you can use the "smartness"
  of your application layer to shard your graph efficiently to your machines
  and let traversals run locally.

- **Encryption Control**: Choose your level of [SSL encryption](Programs/Arangod/Ssl.md)

- [**Auditing**](Security/Auditing/README.md): Keep a detailed log
  of all the important things that happened in ArangoDB.

Also see [What's New in 3.1](ReleaseNotes/NewFeatures31.md).

Version 3.0
-----------

- [**self-organizing cluster**](Architecture/DeploymentModes/Cluster/Architecture.md) with
  synchronous replication, master/master setup, shared nothing
  architecture, cluster management agency.

- Deeply integrated, native [**AQL graph traversal**](../AQL/Graphs/index.html)

- [**VelocyPack**](https://github.com/arangodb/velocypack) as new internal
  binary storage format as well as for intermediate AQL values.

- [**Persistent indexes**](Indexing/Persistent.md) via RocksDB suitable
  for sorting and range queries.

- [**Foxx 3.0**](Foxx/README.md): overhauled JS framework for data-centric
  microservices

- Significantly improved [**Web Interface**](Programs/WebInterface/README.md)
  
Also see [What's New in 3.0](ReleaseNotes/NewFeatures30.md).
