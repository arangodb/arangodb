![ArangoDB-Logo](https://www.arangodb.org/wp-content/uploads/2012/10/logo_arangodb_transp.png)

ArangoDB
========

Master: [![Build Status](https://secure.travis-ci.org/triAGENS/ArangoDB.png?branch=master)](http://travis-ci.org/triAGENS/ArangoDB)
Devel: [![Build Status](https://secure.travis-ci.org/triAGENS/ArangoDB.png?branch=devel)](http://travis-ci.org/triAGENS/ArangoDB)

ArangoDB is a universal open-source database with flexible data model for documents, graphs, and key-values. Build high performance application using a convenient sql-like query language or JavaScript/Ruby extensions. 

Key features include:

* **Schema-free schemata** let you combine the space efficiency of MySQL with the performance power of NoSQL
* Use ArangoDB as an **application server** and fuse your application and database together for maximal throughput
* JavaScript for all: **no language zoo**, you can use one language from your browser to your back-end
* ArangoDB is **multi-threaded** - exploit the power of all your cores
* **Flexible data modeling**: model your data as combination of key-value pairs, documents or graphs - perfect for social relations
* Free **index choice**: use the correct index for your problem, be it a skip list or a n-gram search
* Configurable **durability**: let the application decide if it needs more durability or more performance
* No-nonsense storage: ArangoDB uses all of the power of **modern storage hardware**, like SSD and large caches
* **Powerful query language** (AQL) to retrieve data 
* It is **open source** (Apache Licence 2.0)

For more in-depth information

* read more on the [design goals of ArangoDB](http://www.arangodb.org/2012/03/07/avocadodbs-design-objectives)
* [watch the video](http://vimeo.com/36411892) - Martin Schoenert, architect of ArangoDB, gives an introduction of what the ArangoDB project is about.
* or give it a [try](http://www.arangodb.org/try).


For the Impatient
-----------------

For Mac OSX execute

    brew install arangodb

For Linux use the distribution dependend package manager, see 
[Installation Manual](http://www.arangodb.org/manuals/current/Installing.html)
for details.

For Windows users: We provide an installer script on our 
[download page](http://www.arangodb.org/download).

If the package manager has not already started the ArangoDB server, use

    unix> /path/to/sbin/arangod
    2012-03-30T12:54:19Z [11794] INFO ArangoDB (version 1.x.y) is ready for business
    2012-03-30T12:54:19Z [11794] INFO Have Fun!

`/path/to/sbin` is OS dependent. It will normally by either `/usr/sbin` or `/user/local/sbin`. Point your browser to

    http://localhost:8529/

and select the tab `Shell`. You can now use the Arango shell from within your browser. Alternative, it is available as command-line tool _arangosh_.

    arangosh> db._create("hallo");
    arangosh> db.hallo.save({ world: "earth" });

Congratulations! You have created your first collection called `hallo` and your first document. To verify your achievements

    arangosh> db.hallo.toArray();


More Information
----------------

Please check the
[Installation Manual](http://www.arangodb.org/manuals/current/InstallManual.html)
for installation and compilation instructions.

The
[User Manual](http://www.arangodb.org/manuals/current/UserManual.html)
has an introductory chapter showing the basic operations of ArangoDB.

Or you can use the 
[online tutorial](http://www.arangodb.org/try)
to play with ArangoDB without installing it locally.


Stay in Contact
---------------

Please note that there will be bugs and we'd really appreciate it if
you report them:

    https://github.com/triAGENS/ArangoDB/issues

You can use the Google group for improvements, feature requests, comments 

    http://www.arangodb.org/connect

