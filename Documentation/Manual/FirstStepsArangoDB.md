First Steps with ArangoDB {#FirstStepsArangoDB}
===============================================

@NAVIGATE{UserManualBasics,UserManual,UserManualArangosh}
@EMBEDTOC{FirstStepsArangoDBTOC}

What is ArangoDB? {#FirstStepsArangoDBIntro}
============================================

ArangoDB is a universal open-source database with a flexible data
model for documents, graphs, and key-values. You can easily build high
performance applications using a convenient sql-like query language or
JavaScript/Ruby extensions.

The database server `arangod` stores all documents and serves them
using a REST interface. There are driver for all major language like
Ruby, Python, PHP, JavaScript, and Perl. In the following sections we
will use the JavaScript shell to communicate with the database and
demonstrate some of ArangoDB's features using JavaScript.

Key features include:

- *Schema-free schemata* let you combine the space efficiency of MySQL with
  the performance power of NoSQL
- Use ArangoDB as an *application server* and fuse your application and
  database together for maximal throughput
- *JavaScript for all:* no language zoo, you can use one language from your
  browser to your back-end
- *Flexible data modeling:* model your data as combination of key-value pairs,
  documents or graphs - perfect for social relations
- *Free index choice:* use the correct index for your problem, be it a skip
  list or a n-gram search
- *Configurable durability:* let the application decide if it needs more
  durability or more performance
- *No-nonsense storage:* ArangoDB uses all of the power of modern storage
  hardware, like SSD and large caches
- It is open source (*Apache Licence 2.0*)

For more in-depth information

- read more on the 
  @EXTREF_S{http://www.arangodb.org/2012/03/07/avocadodbs-design-objectives,design goals} 
  of ArangoDB
- @EXTREF_S{http://vimeo.com/36411892,watch the video} - Martin Schoenert, 
  architect of ArangoDB, gives an introduction of what the ArangoDB project 
  is about
- or give it a @EXTRES{http://www.arangodb.org/try,try}

Getting Familiar with ArangoDB {#FirstStepsArangoDBServerStart}
===============================================================

First of all download and install the corresponding RPM or Debian
package or use homebrew on the MacOS X. See the
@EXTREF_S{InstallManual.html, installation manual} for more details.
In case you just want to experiment with ArangoDB you can use the
@EXTREF_S{http://www.arangodb.org/try,on-line} demo without installing
ArangoDB locally.

The ArangoDB database package comes with the following programs:

- `arangod`: The ArangoDB database daemon. This server program is
  intended to run as daemon process and to server the various clients
  connection to the server via TCP / HTTP. See @ref
  UserManualServerStartStop.
- `arango-update`: Used to initialize or update the database.
- `arangosh`: The ArangoDB shell. A client that implements a
  read-eval-print loop (REPL) and provides functions to access and
  administrate the ArangoDB server. See @ref UserManualShellStartStop.
- `arangoimp`: A bulk importer for the ArangoDB server.
  See @ref ImpManual

For Linux:

- visit the official ArangoDB download page at 
  @EXTREF{http://www.arangodb.org/download,http://www.arangodb.org/download}
  and download the correct package for you Linux distribution
- install the package using you favorite package manager
- start up the database server, normally this is down by
  executing `/etc/init.d/arangod start`. The exact command
  depends on your Linux distribution

For MacOS X:

- execute `brew install arangodb`
- initialise (or upgrade) the database `/usr/local/bin/arango-upgrade`
- and start the server using `/usr/local/sbin/arangod &`

After these steps there should be a running instance of `arangod` -
the ArangoDB database server.

    unix> ps auxw | fgrep arangod
    arangodb 14536 0.1 0.6 5307264 23464 s002 S 1:21pm 0:00.18 /usr/local/sbin/arangod

If there is no such process, check the log file
`/var/log/arangodb/arangod.log` for errors.

Exploring Collections and Documents {#FirstStepsArangoDBFirstSteps}
===================================================================

ArangoDB is a database that serves documents to clients.

- A *document* contains zero or more attributes, each of these
  attribute has a value. A value can either be a atomic type, i. e.,
  integer, strings, boolean or a list or an embedded document. Documents
  are normally represented as JSON objects.
- Documents are grouped into *collections*. A collection can contains zero
  or more documents.
- *Queries* are used to extract documents based on filtere criterias;
  queries can be as simple as a query by-example or as complex as a
  joins using many collections or graph structures.
- *Cursors* are used to iterate over the result of a query.
- *Indexes* are used to speed up of searches; there are various different
  types of indexes like hash indexes, geo-indexes, bit-indexes.

If you are familiar with RDBMS then it is safe to compare collections
to tables and documents to rows. However, bringing structure to the
"rows" has many advantages - as you will see later.

Starting the JavaScript shell {#FirstStepsArangoDBConnecting}
-------------------------------------------------------------

The easiest way to connect to the database is the JavaScript shell
`arangosh`. You can either start it from the command-line or as an
embedded version in the browser. Using the command-line tool has the
advantage that you can use auto-completion.

    unix> arangosh --server.password ""
					   _     
      __ _ _ __ __ _ _ __   __ _  ___  ___| |__  
     / _` | '__/ _` | '_ \ / _` |/ _ \/ __| '_ \ 
    | (_| | | | (_| | | | | (_| | (_) \__ \ | | |
     \__,_|_|  \__,_|_| |_|\__, |\___/|___/_| |_|
			   |___/                 

    Welcome to arangosh 1.x.y. Copyright (c) 2012 triAGENS GmbH.
    Using Google V8 3.9.4 JavaScript engine.
    Using READLINE 6.1.

    Connected to Arango DB 127.0.0.1:8529 Version 1.x.y

    ------------------------------------- Help -------------------------------------
    Predefined objects:                                                 
      arango:                                ArangoConnection           
      db:                                    ArangoDatabase             
    Example:                                                            
     > db._collections();                    list all collections       
     > db.<coll_name>.all();                 list all documents         
     > id = db.<coll_name>.save({ ... });    save a document            
     > db.<coll_name>.remove(<_id>);         delete a document          
     > db.<coll_name>.document(<_id>);       get a document             
     > help                                  show help pages            
     > helpQueries                           query help                 
     > exit                                                             
    arangosh>

This gives you a prompt, where you can issue JavaScript commands.

The standard setup does not require a password. Depending on you
setup, you might need to specify the endpoint, username and password
in order to run the shell on your system. You can use the options
`--server.endpoint`, `--server.username` and `--server.password` for
this. If you do not specify a password, arangosh will prompt for one.

    unix> arangosh --server.endpoint tcp://127.0.0.1:8529 --server.username root

Querying For Documents {#FirstStepsArangoDBQuerying}
----------------------------------------------------

All documents are stored in collections. All collections are stored in a
database. The database object is accessible there the variable `db` from
the module 

    arangosh> db = require("org/arangodb").db;
    [object ArangoDatabase]

Creating a collection is simple. You can use the `_create` method
of the `db` variable.

    arangosh> db._create("example");
    [ArangoCollection 70628, "example" (status loaded)]

After the collection has been create you can easily access it using
the path `db.example`. The collection currently shows as `loaded`,
meaning that its loaded into memory. If you restart the server and
access the collection again, it will now show as `unloaded`. You can
also manually unload a collection

    arangosh> db.example.unload();
    arangosh> db.example;
    [ArangoCollection 70628, "example" (status unloaded)]

Whenever you use a collection, ArangoDB will automatically load it
into memory for you.

In order to create new documents in a collection, use the `save`
operator. 

    arangosh> db.example.save({ Hallo : "World" });
    { error : false, _id : "70628/1512420", _rev : 1512420 }
    arangosh> db.example.save({ name : "Mustermann", age : 29 });
    { error : false, _id : "70628/1774564", _rev : 1774564 }
    arangosh> db.example.save({ name : "Musterfrau", age : 31 });
    { error : false, _id : "70628/1774565", _rev : 1774565 }

Just storing documents would be no fun. We now want to select some of
the stored documents again.  In order to select all elements of a
collection, one can use the `all` operator. Because this might return
a lot of data, we switch on pretty printing before.

    arangosh> start_pretty_print();
    use pretty printing

The command `stop_pretty_print` will switch off pretty printing again.
Now extract all elements.

    arangosh> db.example.all().toArray()
    [
      { 
	_id : "4538791/6308263", 
	_rev : 6308263, 
	age : 31, 
	name : "Musterfrau"
       }, 
      { 
	_id : "4538791/6242727", 
	_rev : 6242727, 
	age : 29, 
	name : "Mustermann"
       }, 
      { 
	_id : "4538791/5980583", 
	_rev : 5980583, 
	Hallo : "World"
       }
    ]

The last document was a mistake, so let's delete it

    arangosh> db.example.remove("4538791/5980583")
    true
    arangosh> db.example.all().toArray()
    [
      { 
	_id : "4538791/6308263", 
	_rev : 6308263, 
	age : 31, 
	name : "Musterfrau"
       }, 
      { 
	_id : "4538791/6242727", 
	_rev : 6242727, 
	age : 29, 
	name : "Mustermann"
       }
    ]

Now we want to look for a person with a given name, we can use
`byExample` for this. The methods returns a list of documents
matching a given example.

    arangosh> db.example.byExample({ name: "Musterfrau" }).toArray()
    [
      { 
	_id : "4538791/6308263", 
	_rev : 6308263, 
	age : 31, 
	name : "Musterfrau"
       }
    ]

While the `byExample` works very well for simple queries where you
`and` conditions together, the above syntax becomes messy for joins
and `or` conditions. Therefore, ArangoDB also supports a full-blown
query language.

    arangosh> db._query('FOR user IN example FILTER user.name == "Musterfrau" return user').toArray()
    [
      { 
	_id : "4538791/6308263", 
	_rev : 6308263, 
	age : 31, 
	name : "Musterfrau"
       }
    ]

Search for all persons over 30.

    arangosh> db._query('FOR user IN example FILTER user.age > 30" return user').toArray()
    [
      { 
	_id : "4538791/6308263", 
	_rev : 6308263, 
	age : 31, 
	name : "Musterfrau"
       }
    ]

You can learn all about the query language @ref Aql "here". Note that
`_query` is a short-cut for `_createStatement` and `execute`. We will
come back to these functions when we talk about cursors.

Details about Starting the ArangoDB Server {#FirstStepsServerStartStop}
=======================================================================

The ArangoDB database server has two modes of operation: as server, where it
will answer to client requests, and an emergency console, in which you can
access the database directly. The latter - as the name suggests - should 
only be used in case of an emergency, for example, a corrupted
collection. Using the emergency console allows you to issue all commands
normally available in actions and transactions. When starting the server in
emergency console mode, the server cannot handle any client requests.

You should never start more than one server using the same database directory,
independent from the mode of operation. Normally ArangoDB will prevent
you from doing this by placing a lockfile in the database directory and
not allowing a second ArangoDB instance to use the same database directory
if a lockfile is already present.

The following command starts the ArangoDB database in server mode. You will
be able to access the server using HTTP requests on port 8529. See @ref
FirstStepsServerStartStopOptions "below" for a list of frequently used
options, see @ref CommandLine "here" for a complete list.

    unix> /usr/local/sbin/arangod /tmp/vocbase
    20ZZ-XX-YYT12:37:08Z [8145] INFO using built-in JavaScript startup files
    20ZZ-XX-YYT12:37:08Z [8145] INFO ArangoDB (version 1.x.y) is ready for business
    20ZZ-XX-YYT12:37:08Z [8145] INFO Have Fun!

After starting the server, point your favorite browser to:

    http://localhost:8529/

to access the administration front-end.

To start the server at system boot time, you should use one of the 
pre-rolled packages that will install the necessary start / stop
scripts for ArangoDB. To start and stop the server manually, you can
use the start / stop script like this (provided the start / stop script
is located in /etc/init.d/arangod, the command actual name and invocation are
platform-dependent):

    /etc/init.d/arangod start
 
To stop the server, you can use the command

    /etc/init.d/arangod stop

You may require root privileges to execute these commands.

If you compiled ArangoDB from source and did not use any installation
package, or you are using non-default locations and/or multiple ArangoDB
instances on the same host, you may want to start the server process 
manually. You can do so by invoking the arangod binary from the command
line as shown before. To stop the database server gracefully, you can
either pressCTRL-C or by send the SIGINT signal to the server process. 
On many systems, this can be achieved with the following command:

    kill -2 `pidof arangod`

Frequently Used Options {#FirstStepsServerStartStopOptions}
-----------------------------------------------------------

The following command-line options are frequently used. For a full
list of options see @ref CommandLine "here".

@CMDOPT{@CA{database-directory}}

Uses the @CA{database-directory} as base directory. There is an
alternative version available for use in configuration files, see @ref
CommandLineArango "here".


@copydetails triagens::rest::ApplicationServer::_options


@CMDOPT{--log @CA{level}}

Allows the user to choose the level of information which is logged by
the server. The @CA{level} is specified as a string and can be one of
the following values: fatal, error, warning, info, debug, trace.  For
more information see @ref CommandLineLogging "here".


@copydetails triagens::rest::ApplicationEndpointServer::_endpoints


@copydetails triagens::rest::ApplicationEndpointServer::_disableAuthentication


@copydetails triagens::rest::ApplicationEndpointServer::_keepAliveTimeout


@CMDOPT{--daemon}

Runs the server as a daemon (as a background process).
