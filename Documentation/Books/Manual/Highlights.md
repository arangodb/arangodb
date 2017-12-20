Highlights
==========

Version 3.3
-----------

### Enterprise Edition

- [**Datacenter-to-Datacenter Replication**](Deployment/DC2DC.md): 
  Every company needs a disaster recovery plan for all important systems.
  This is true from small units like single processes running in some 
  container to the largest distributed architectures. For databases in 
  particular this usually involves a mixture of fault-tolerance, 
  redundancy, regular backups and emergency plans. The larger a 
  data store, the more difficult it is to come up with a good strategy.

  Therefore, it is desirable to be able to run a distributed database 
  in one data-center and replicate all transactions to another 
  data-center in some way. Often, transaction logs are shipped 
  over the network to replicate everything in another, identical 
  system in the other data-center. Some distributed data stores have 
  built-in support for multiple data-center awareness and can 
  replicate between data-centers in a fully automatic fashion.

  ArangoDB 3.3 takes an evolutionary step forward by introducing 
  multi-data-center support, which is asynchronous data-center to 
  data-center replication. Our solution is asynchronous and scales 
  to arbitrary cluster sizes, provided your network link between 
  the data-centers has enough bandwidth. It is fault-tolerant 
  without a single point of failure and includes a lot of 
  metrics for monitoring in a production scenario.

- [**Encrypted Backups**](#):
  Arangodump can create backups encrypted with a secret key using AES256
  block cipher.

### All Editions

- [**Server-global Replication**](#):
  In addition to per-database replication, there is now an additional
  `globalApplier`. Start the global replication on the slave once and all
  current and future databases will be replicated from the master to the
  slave automatically.

- [**Asynchronous Failover**](#):
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

- **Enterprise**: Working with some of our largest customers, we’ve added
  further security and scalability features to ArangoDB Enterprise like
  [LDAP integration](Administration/Configuration/Ldap.md),
  [Encryption at Rest](Administration/Encryption/README.md), and the brand new
  [Satellite Collections](Administration/Replication/Synchronous/Satellites.md).

Also see [What's New in 3.2](ReleaseNotes/NewFeatures32.md).

Version 3.1
-----------

- [**SmartGraphs**](Graphs/SmartGraphs/README.md): Scale with graphs to a
  cluster and stay performant. With SmartGraphs you can use the "smartness"
  of your application layer to shard your graph efficiently to your machines
  and let traversals run locally.

- **Encryption Control**: Choose your level of [SSL encryption](Administration/Configuration/SSL.md)

- [**Auditing**](Administration/Auditing/README.md): Keep a detailed log
  of all the important things that happened in ArangoDB.

Also see [What's New in 3.1](ReleaseNotes/NewFeatures31.md).

Version 3.0
-----------

- [**self-organizing cluster**](Scalability/Architecture.md) with
  synchronous replication, master/master setup, shared nothing
  architecture, cluster management agency.

- Deeply integrated, native [**AQL graph traversal**](../AQL/Graphs/index.html)

- [**VelocyPack**](https://github.com/arangodb/velocypack) as new internal
  binary storage format as well as for intermediate AQL values.

- [**Persistent indexes**](Indexing/Persistent.md) via RocksDB suitable
  for sorting and range queries.

- [**Foxx 3.0**](Foxx/README.md): overhauled JS framework for data-centric
  microservices

- Significantly improved [**Web Interface**](Administration/WebInterface/README.md)
  
Also see [What's New in 3.0](ReleaseNotes/NewFeatures30.md).
