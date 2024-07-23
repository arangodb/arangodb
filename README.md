<picture>
  <source media="(prefers-color-scheme: dark)" srcset="https://user-images.githubusercontent.com/7819991/218699214-264942f9-b020-4f50-b1a6-3363cdc0ddc9.svg" width="638" height="105">
  <source media="(prefers-color-scheme: light)" srcset="https://user-images.githubusercontent.com/7819991/218699215-b9b4a465-45f8-4db9-b5a4-ba912541e017.svg" width="638" height="105">
  <img alt="Two stylized avocado halves and the product name." src="https://user-images.githubusercontent.com/7819991/218697980-26ffd7af-cf29-4365-8a5d-504b850fc6b1.png" width="638" height="105">
</picture>

ArangoDB
========

ArangoDB is a scalable graph database system to drive value from connected data,
faster. Native graphs, an integrated search engine, and JSON support, via a
single query language. ArangoDB runs on-prem, in the cloud – anywhere.

ArangoDB Cloud Service
----------------------

The [ArangoGraph Insights Platform](https://cloud.arangodb.com/home) is the
simplest way to run ArangoDB. You can create deployments on all major cloud
providers in many regions with ease.

Getting Started
---------------

- [ArangoDB University](https://university.arangodb.com/)
- [Free Udemy Course](https://www.udemy.com/course/getting-started-with-arangodb)
- [Training Center](https://www.arangodb.com/learn/)
- [Documentation](https://docs.arangodb.com/)

For the impatient:

- Test ArangoDB in the cloud with [ArangoGraph](https://cloud.arangodb.com/home) for free.

- Alternatively, [download](https://www.arangodb.com/download) and install ArangoDB.
  Start the server `arangod` if the installer did not do it for you.

  Or start ArangoDB in a Docker container:

      docker run -e ARANGO_ROOT_PASSWORD=test123 -p 8529:8529 -d arangodb

  Then point your browser to `http://127.0.0.1:8529/`.

Key Features of ArangoDB
------------------------

**Native Graph** - Store both data and relationships, for faster queries even
with multiple levels of joins and deeper insights that simply aren't possible
with traditional relational and document database systems.

**Document Store** - Every node in your graph is a JSON document:
flexible, extensible, and easily imported from your existing document database.

**ArangoSearch** - Natively integrated cross-platform indexing, text-search and
ranking engine for information retrieval, optimized for speed and memory.

ArangoDB is available in a free and open-source **Community Edition**, as well
as a commercial **Enterprise Edition** with additional features.

### Community Edition features

- **Horizontal scalability**: Seamlessly shard your data across multiple machines.
- **High availability** and **resilience**: Replicate data to multiple cluster
  nodes, with automatic failover.
- **Flexible data modeling**: Model your data as combination of key-value pairs,
  documents, and graphs as you see fit for your application.
- Work **schema-free** or use **schema validation** for data consistency.
  Store any type of data - date/time, geo-spatial, text, nested.
- **Powerful query language** (_AQL_) to retrieve and modify data - from simple
  CRUD operations, over complex filters and aggregations, all the way to joins,
  graphs, and ranked full-text search.
- **Transactions**: Run queries on multiple documents or collections with
  optional transactional consistency and isolation.
- **Data-centric microservices**: Unify your data storage logic, reduce network
  overhead, and secure sensitive data with the _ArangoDB Foxx_ JavaScript framework.
- **Fast access to your data**: Fine-tune your queries with a variety of index
  types for optimal performance. ArangoDB is written in C++ and can handle even
  very large datasets efficiently.
- Easy to use **web interface** and **command-line tools** for interaction
  with the server.

### Enterprise Edition features

Focus on solving enterprise-scale problems for mission critical workloads using
secure graph data. The Enterprise Edition has all the features of the
Community Edition and offers additional features for performance, compliance,
and security, as well as further query capabilities.

- Smartly shard and replicate graphs and datasets with features like
  **EnterpriseGraphs**, **SmartGraphs**, and **SmartJoins** for lightning fast
  query execution.
- Combine the performance of a single server with the resilience of a cluster
  setup using **OneShard** deployments.
- Increase fault tolerance with **Datacenter-to-Datacenter Replication** and
  and create incremental **Hot Backups** without downtime.
- Enable highly secure work with **Encryption 360**, enhanced **Data Masking**, 
  and detailed **Auditing**.
- Perform **parallel graph traversals**.
- Use ArangoSearch **search highlighting** and **nested search** for advanced
  information retrieval.

Latest Release
--------------

Packages for all supported platforms can be downloaded from
<https://www.arangodb.com/download/>.

For what's new in ArangoDB, see the Release Notes in the
[Documentation](https://docs.arangodb.com/).

Stay in Contact
---------------

- Please use GitHub for feature requests and bug reports:
  [https://github.com/arangodb/arangodb/issues](https://github.com/arangodb/arangodb/issues)

- Ask questions about AQL, usage scenarios, etc. on StackOverflow:
  [https://stackoverflow.com/questions/tagged/arangodb](https://stackoverflow.com/questions/tagged/arangodb)

- Chat with the community and the developers on Slack:
  [https://arangodb-community.slack.com/](https://arangodb-community.slack.com/)

- Learn more about ArangoDB with our YouTube channel: 
  [https://www.youtube.com/@ArangoDB](https://www.youtube.com/@ArangoDB)

- Follow us on Twitter to stay up to date:
  [https://twitter.com/arangodb](https://twitter.com/arangodb)

- Find out more about our community: [https://www.arangodb.com/community](https://www.arangodb.com/community/)
