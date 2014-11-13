![ArangoDB-Logo](https://www.arangodb.com/wp-content/uploads/2012/10/logo_arangodb_transp.png)

ArangoDB
========

Master: [![Build Status](https://secure.travis-ci.org/triAGENS/ArangoDB.png?branch=master)](http://travis-ci.org/triAGENS/ArangoDB)
Devel: [![Build Status](https://secure.travis-ci.org/triAGENS/ArangoDB.png?branch=devel)](http://travis-ci.org/triAGENS/ArangoDB)

ArangoDB is a multi-purpose, open-source database with flexible data models for documents, graphs, and key-values. Build high performance applications using a convenient SQL-like query language or JavaScript extensions. Use ACID transactions if you require them. Scale horizontally and vertically with a few mouse clicks.

Key features include:

* **Schema-free schemata** let you combine the space efficiency of MySQL with the performance power of NoSQL
* Use ArangoDB as an **application server** and fuse your application and database together for maximal throughput
* JavaScript for all: **no language zoo**, you can use one language from your browser to your back-end
* ArangoDB is **multi-threaded** - exploit the power of all your cores
* **Flexible data modelling**: model your data as combination of key-value pairs, documents or graphs - perfect for social relations
* Free **index choice**: use the correct index for your problem, be it a skip list or a fulltext search
* Configurable **durability**: let the application decide if it needs more durability or more performance
* No-nonsense storage: ArangoDB uses all of the power of **modern storage hardware**, like SSD and large caches
* **Powerful query language** (AQL) to retrieve and modify data 
* **Transactions**: run queries on multiple documents or collections with optional transactional consistency and isolation
* **Replication** and **Sharding**: set up the database in a master-slave configuration or spread bigger datasets across multiple servers
* It is **open source** (Apache Licence 2.0)

For more in-depth information

* read more on the [design goals of ArangoDB](http://www.arangodb.com/2012/03/07/avocadodbs-design-objectives)
* [watch the video](http://vimeo.com/36411892) - Martin Schoenert, architect of ArangoDB, gives an introduction of what the ArangoDB project is about.
* or give it a [try](http://www.arangodb.com/try).


For the Impatient
-----------------

For Mac OSX users: execute

    brew install arangodb

For Windows and Linux users: use the installer script or distribution package
from our [download page](http://www.arangodb.com/download).

If the package manager has not already started the ArangoDB server, use the 
following command to start it.

    unix> /path/to/sbin/arangod
    2012-03-30T12:54:19Z [11794] INFO ArangoDB (version 2.x.y) is ready for business
    2012-03-30T12:54:19Z [11794] INFO Have Fun!

`/path/to/sbin` is OS dependent. It will normally be either `/usr/sbin` or `/user/local/sbin`. 

To access ArangoDB in your browser, open the following URL

    http://localhost:8529/

and select `Tools / JS Shell`. You can now use the Arango shell from within your browser. 

Alternatively, a scriptable shell is available as a command-line tool _arangosh_.

    arangosh> db._create("hello");
    arangosh> db.hello.save({ world: "earth" });

Congratulations! You have created your first collection named `hello` and your first document. 
To verify your achievements, type:

    arangosh> db.hello.toArray();


More Information
----------------

Please check the
[Installation Manual](https://docs.arangodb.com/Installing/README.html)
for installation and compilation instructions.

The
[User Manual](https://docs.arangodb.com/FirstSteps/README.html)
has an introductory chapter showing the basic operations of ArangoDB.

Or you can use the 
[online tutorial](https://www.arangodb.com/tryitout)
to play with ArangoDB without installing it locally.


Stay in Contact
---------------

Please note that there will be bugs and we'd really appreciate it if
you report them:

[https://github.com/triAGENS/ArangoDB/issues](https://github.com/triAGENS/ArangoDB/issues)

You can use the Google group for improvements, feature requests, comments 

[http://www.arangodb.com/community](https://www.arangodb.com/community)


Citing ArangoDB
---------------
Please kindly cite ArangoDB in your publications if it helps your research:

```bibtex
@misc{ArangoDB2014,
   Author = {ArangoDB},
   Title = { {ArangoDB 2.3}: An Open source, multi-purpose database supporting flexible data models for documents, graphs, and key-values.},
   Year  = {2014},
   Howpublished = {\url{http://arangodb.com/}
}
```
