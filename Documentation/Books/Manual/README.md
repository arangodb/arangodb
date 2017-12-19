!BOOK ArangoDB VERSION_NUMBER Documentation

Welcome to the ArangoDB documentation!

New and eager to try out ArangoDB? Start right away with our beginner's guide:
[Getting Started](GettingStarted/README.md)

The documentation is organized in four handbooks:

- This manual describes ArangoDB and its features in detail for you as a user,
  developer and administrator.
- The [AQL handbook](../AQL/index.html) explains ArangoDB's query language AQL.
- The [HTTP handbook](../HTTP/index.html) describes the internal API of ArangoDB
  that is used to communicate with clients. In general, the HTTP handbook will be
  of interest to driver developers. If you use any of the existing drivers for
  the language of your choice, you can skip this handbook.
- Our [cookbook](../cookbook/index.html) with recipes for specific problems and
  solutions.

Features are illustrated with interactive usage examples; you can cut'n'paste them
into [arangosh](Administration/Arangosh/README.md) to try them out. The HTTP
[REST-API](../HTTP/index.html) for driver developers is demonstrated with cut'n'paste
recepies intended to be used with the [cURL](http://curl.haxx.se). Drivers may provide
their own examples based on these .js based examples to improve understandeability
for their respective users, i.e. for the [java driver](https://github.com/arangodb/arangodb-java-driver#learn-more)
some of the samples are re-implemented.

!SUBSECTION Overview

ArangoDB is a multi-model, open-source database with flexible data models for documents, graphs, and key-values. Build high performance applications using a convenient SQL-like query language or JavaScript extensions. Use ACID transactions if you require them. Scale horizontally and vertically with a few mouse clicks.

Key features include:

* installing ArangoDB on a [**cluster**](Deployment/README.md) is as easy as installing an app on your mobile
* [**Flexible data modeling**](DataModeling/README.md): model your data as combination of key-value pairs, documents or graphs - perfect for social relations
* [**Powerful query language**](../AQL/index.html) (AQL) to retrieve and modify data 
* Use ArangoDB as an [**application server**](Foxx/README.md) and fuse your application and database together for maximal throughput
* [**Transactions**](Transactions/README.md): run queries on multiple documents or collections with optional transactional consistency and isolation
* [**Replication** and **Sharding**](Administration/README.md): set up the database in a master-slave configuration or spread bigger datasets across multiple servers
* Configurable **durability**: let the application decide if it needs more durability or more performance
* No-nonsense storage: ArangoDB uses all of the power of **modern storage hardware**, like SSD and large caches
* JavaScript for all: **no language zoo**, you can use one language from your browser to your back-end
* It is **open source** (Apache License 2.0)

!SUBSECTION Community

If you have questions regarding ArangoDB, Foxx, drivers, or this documentation don't hesitate to contact us on:

- [GitHub](https://github.com/arangodb/arangodb/issues) for issues and misbehavior or [pull requests](https://www.arangodb.com/community/)
- [Google Groups](https://groups.google.com/forum/?hl=de#!forum/arangodb) for discussions about ArangoDB in general or to announce your new Foxx App
- [StackOverflow](http://stackoverflow.com/questions/tagged/arangodb) for questions about AQL, usage scenarios etc.
- [Slack](http://slack.arangodb.com), our community chat

When reporting issues, please describe:

- the environment you run ArangoDB in
- the ArangoDB version you use
- whether you're using Foxx
- the client you're using
- which parts of the documentation you're working with (link)
- what you expect to happen
- what is actually happening

We will respond as soon as possible.
