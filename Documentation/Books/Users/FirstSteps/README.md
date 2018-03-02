First Steps in ArangoDB
=======================

For installation instructions, please refer to the 
[Installation Manual](../Installing/README.md).

As you know from the introduction ArangoDB is a multi-model open-source
Database. You can see the Key features below or look directly at the programs in
the ArangoDB package. 

Key features include:

* *Schema-free schemata*: Lets you combine the space efficiency of MySQL with
  the performance power of NoSQL
* *Application server*: Use ArangoDB as an application server and fuse your
  application and database together for maximal throughput
* *JavaScript for all*: No language zoo, you can use one language from your
  browser to your back-end
* *Flexible data modeling*: Model your data as combinations of key-value pairs,
  documents or graphs - perfect for social relations
* *Free index choice*: Use the correct index for your problem, may it be a skip
  list or a fulltext search 
* *Configurable durability*: Let the application decide if it needs more
  durability or more performance
* *No-nonsense storage*: ArangoDB uses all of the power of modern storage
  hardware, like SSD and large caches
* *Powerful query language* (AQL) to retrieve and modify data 
* *Transactions*: Run queries on multiple documents or collections with 
  optional transactional consistency and isolation
* *Replication*: Set up the database in a master-slave configuration
* It is open source (*Apache License 2.0*)

For more in-depth information:

* Read more on the 
  [Design Goals](https://www.arangodb.com/2012/03/07/avocadodbs-design-objectives) 
  of ArangoDB
* [Watch the video](http://vimeo.com/36411892): Martin Schönert, 
  architect of ArangoDB, gives an introduction of what the ArangoDB project 
  is about
* Or give it a [try](https://www.arangodb.com/tryitout)


ArangoDB programs
-----------------

The ArangoDB package comes with the following programs:

* _arangod_: The ArangoDB database daemon. This server program is
  intended to run as a daemon process and to serve the various clients
  connection to the server via TCP / HTTP. See [Details about the ArangoDB Server](../FirstSteps/Arangod.md) 
* _arangosh_: The ArangoDB shell. A client that implements a
  read-eval-print loop (REPL) and provides functions to access and
  administrate the ArangoDB server. See [Details about the ArangoDB Shell](../FirstSteps/Arangosh.md).
* _arangoimp_: A bulk importer for the ArangoDB server.
  See [Details about Arangoimp](../HttpBulkImports/Arangoimp.md).
* _arangodump_: A tool to create backups of an ArangoDB database. See
   [Details about Arangodump](../HttpBulkImports/Arangodump.md).
* _arangorestore_: A tool to reload data from a backup into an ArangoDB database.
  See [Details about Arangorestore](../HttpBulkImports/Arangorestore.md)
* _arango-dfdb_: A datafile debugger for ArangoDB. It is intended to be
  used primarily during development of ArangoDB
