# ArangoDB
[![Build Status](https://secure.travis-ci.org/triAGENS/ArangoDB.png)](http://travis-ci.org/triAGENS/ArangoDB)

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
* or  give it a try.

## Compilation

Please check the
[Installation Manual](http://www.arangodb.org/manuals/1.0.0/InstallManual.html)
for installation and compilation instructions.

### Mac OS X Hints

On Mac OS X you can install ArangoDB using the packagemanager [Homebrew](http://mxcl.github.com/homebrew/):

* `brew install arangodb` (use `--HEAD` in order to build ArangoDB from current master)

This will install ArangoDB and all dependencies. Note that the server will be installed as

    /usr/local/sbin/arangod

The ArangoDB shell will be install as

    /usr/local/bin/arangosh

## First Steps

Start the server:

    > /usr/sbin/arangod
    2012-03-30T12:54:19Z [11794] INFO ArangoDB (version 1.x.y) is ready for business
    2012-03-30T12:54:19Z [11794] INFO Have Fun!

Start the shell in another windows:

    > /usr/bin/arangosh
                                           _     
      __ _ _ __ __ _ _ __   __ _  ___  ___| |__  
     / _` | '__/ _` | '_ \ / _` |/ _ \/ __| '_ \ 
    | (_| | | | (_| | | | | (_| | (_) \__ \ | | |
     \__,_|_|  \__,_|_| |_|\__, |\___/|___/_| |_|
                           |___/                 

    Welcome to arangosh 1.x.y. Copyright (c) 2012 triAGENS GmbH.
    Using Google V8 3.9.4.0 JavaScript engine.
    Using READLINE 6.1.

    Connected to Arango DB 127.0.0.1:8529 Version 1.x.y

    arangosh> db._create("examples")
    [ArangoCollection 106097, "examples]

    arangosh> db.examples.save({ Hallo: "World" });
    {"error":false,"_id":"106097/2333739","_rev":2333739}

    arangosh> db.examples.all();
    [{ _id : "82883/1524675", _rev : 1524675, Hallo : "World" }]

## Caveat

Please note that this is an early version of ArangoDB. There will be
bugs and we'd really appreciate it if you [report](https://github.com/triAGENS/ArangoDB/issues")
them:

  https://github.com/triAGENS/ArangoDB/issues
