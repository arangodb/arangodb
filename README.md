![ArangoDB-Logo](https://www.arangodb.com/docs/assets/arangodb_logo_2016_inverted.png)

ArangoDB
========

Slack: [![ArangoDB-Logo](https://slack.arangodb.com/badge.svg)](https://slack.arangodb.com)

ArangoDB is a multi-model, open-source database with flexible data models for
documents, graphs, and key-values. Build high performance applications using a
convenient SQL-like query language or JavaScript extensions. Use ACID
transactions if you require them. Scale horizontally with a few mouse clicks.

The supported data models can be mixed in queries and allow ArangoDB to be the
aggregation point for your data.

Check out our [training center](https://www.arangodb.com/arangodb-training-center/)
to get started and see the full [documentation](https://www.arangodb.com/docs/stable/)
to dive deeper.

For the impatient:

- Start the ArangoDB Docker container

      docker run -e ARANGO_ROOT_PASSWORD=test123 -p 8529:8529 -d arangodb/arangodb

- Alternatively, [download](https://www.arangodb.com/download) and install ArangoDB.
  Start the server `arangod` if the installer did not do it for you.

- Point your browser to `http://127.0.0.1:8529/`

Key Features in ArangoDB
------------------------

- **Multi-Model**: Documents, graphs and key-value pairs — model your data as
  you see fit for your application.
- **Joins**: Conveniently join what belongs together for flexible ad-hoc
  querying, less data redundancy.
- **Transactions**: Easy application development keeping your data consistent
  and safe. No hassle in your client.

Here is an AQL query that makes use of all those features:

![AQL Query Example](https://www.arangodb.com/docs/assets/aql_query_with_traversal.png)

Joins and transactions are key features for flexible, secure data designs,
widely used in relational databases but lacking in many NoSQL products. However,
there is no need to forgo them in ArangoDB. You decide how and when to use joins
and strong consistency guarantees, without sacrificing performance and scalability. 

Furthermore, ArangoDB offers a JavaScript framework called [Foxx](https://www.arangodb.com/foxx)
that is executed in the database server with direct access to the data. Build your
own data-centric microservices with a few lines of code. By extending the HTTP API
with user code written in JavaScript, ArangoDB can be turned into a strict
schema-enforcing persistence engine.

Other features of ArangoDB include:

- Use a **data-centric microservices** approach with ArangoDB Foxx and fuse your
  application-logic and database together for maximal throughput
- JavaScript for all: **no language zoo**, you can use one language from your
  browser to your back-end
- **Flexible data modeling**: model your data as combination of key-value pairs,
  documents or graphs - perfect for social relations
- Different **storage engines**: ArangoDB provides a storage engine for mostly
  in-memory operations and an alternative storage engine based on RocksDB which 
  handle datasets that are much bigger than RAM.
- **Powerful query language** (AQL) to retrieve and modify data 
- **Transactions**: run queries on multiple documents or collections with
  optional transactional consistency and isolation
- **Replication** and **Sharding**: set up the database in a master-slave
  configuration or spread bigger datasets across multiple servers
- Configurable **durability**: let the application decide if it needs more
  durability or more performance
- **Schema-free schemata** let you combine the space efficiency of MySQL with the
  performance power of NoSQL
- Free **index choice**: use the correct index for your problem, be it a skiplist 
  or a fulltext search
- ArangoDB is **multi-threaded** - exploit the power of all your cores
- It is **open source** (Apache License 2.0)

For more in-depth information read the
[design goals of ArangoDB](https://www.arangodb.com/2012/03/avocadodbs-design-objectives/)

Latest Release
--------------

Packages for all supported platforms can be downloaded from
[https://www.arangodb.com/download](https://www.arangodb.com/download/).

Please also check [what's new in ArangoDB](https://www.arangodb.com/docs/stable/release-notes.html).

More Information
----------------

See our documentation for detailed
[installation & compilation instructions](https://www.arangodb.com/docs/stable/installation.html).

There is an [introductory chapter](https://www.arangodb.com/docs/stable/getting-started.html)
showing the basic operation of ArangoDB. To learn ArangoDB's query language check out the
[AQL tutorial](https://www.arangodb.com/docs/stable/aql/tutorial.html).

Stay in Contact
---------------

We really appreciate feature requests and bug reports. Please use our Github
issue tracker for reporting them:

[https://github.com/arangodb/arangodb/issues](https://github.com/arangodb/arangodb/issues)

You can use our Google group for improvements, feature requests, comments:

[https://www.arangodb.com/community](https://www.arangodb.com/community)

StackOverflow is great for questions about AQL, usage scenarios etc.

[https://stackoverflow.com/questions/tagged/arangodb](https://stackoverflow.com/questions/tagged/arangodb)

To chat with the community and the developers we offer a Slack chat:

[https://slack.arangodb.com/](https://slack.arangodb.com/)
