New Features in ArangoDB 1.4 {#NewFeatures14}
=============================================

@NAVIGATE_NewFeatures14
@EMBEDTOC{NewFeatures14TOC}

Features and Improvements {#NewFeatures14Introduction}
======================================================

The following list shows in detail which features have been added or improved in
ArangoDB 1.4.  ArangoDB 1.4 also contains several bugfixes that are not listed
here - see the @EXTREF{https://github.com/triAGENS/ArangoDB/blob/devel/CHANGELOG,CHANGELOG} 
for details.

Foxx {#NewFeatures14Foxx}
-------------------------

### Foxx Improvements

Foxx has now left the feature preview state and is now a full feature. It is not backwards
compatible with the feature preview. Please check the Foxx documentation for details.
Some of the most important pitfalls when upgrading:

* `Foxx.Application` is now `Foxx.Controller`
* In your manifest file: `apps` should now be `controllers`

*All future changes will be handled with deprecations beforehand.*

There are a lot of new features including JSDoc Style annotations, an authentication module,
enhanced standard Repository, new route annotations and a possibility to annotate all routes
of a controller at once.

### Foxx Manager

ArangoDB builds now ship with an additional binary `foxx-manager` that can be used
to conveniently manage Foxx application in an ArangoDB instance. To make it easy to 
start with Foxx, the `foxx-manager` provides commands to download example Foxx
applications from a central repository at `https://github.com/arangodb/foxx-apps`,
and install them locally:

    > foxx-manager update
    > foxx-manager install hello-foxx /hello-foxx

This will first mirror the central list of available Foxx applications locally, and
after that make the `hello-foxx` application available in the local ArangoDB database
under the URL prefix `/hello-foxx`.

Multiple Databases {#NewFeatures14MultipleDatabases}
----------------------------------------------------

Traditionally ArangoDB provided _collections_, but no way of grouping them. 
When an ArangoDB server was used for multiple applications, there could be collection 
name clashes, so this was worked around by prefixing collection names with unique 
application names etc.

Since version 1.4, ArangoDB provides _databases_ as the highest level grouping element. 
Multiple databases can exist in parallel, and each collection belongs
to exactly one database. Collection names need to be unique within their database, but
not globally. Thus it is now possible to use the same collection name in different
databases. 

Individual databases are kept relatively separate by ArangoDB. For example, every
databases has its own system collections (e.g. `_users`, `_replication`). This allows
setting up different users for different databases. Replication is also configured on
a per-database level. Other data stored in system collections such as graphs and
AQL user function definitions are also database-specific. 

Server-side actions (including transactions and Foxx) also belong to exactly one database. 
They are only allowed to access data from their own database, and must not switch to another.
AQL queries can also access collections in the current database only. There is no way
to refer to collections of another than the current database.

There is no intention to offer multi-database or cross-database operations in future 
releases.

By default, ArangoDB comes with one database, which is named `_system`. This database
will always be there. Installations that upgrade from older versions to 1.4 will have
all their collections be moved into the `_system` database during the upgrade procedure.

The `_system` database cannot be dropped, though all user-defined collections in it
can be dropped normally if required. All database management operations such as
creating new databases, dropping databases, and retrieving the list of existing databases
can only be carried out from within the `_system` database. Fetching the server logs 
(via the `/_api/log` API) is also restricted the `_system` database.

All command-line tools (e.g. _arangosh_, _arangoimp_, _foxx-manager_) will connect to the 
`_system` database by default. They also provide the option `--server.database` to connect 
to a different database, e.g.:

    > arangosh --server.database mydb --server.endpoint tcp://127.0.0.1:8529

From within _arangosh_, the name of the current database can always be retrieved using the
`db._name()` method:

    arangosh> db._name(); 
    _system

To create a new database, use the `db._createDatabase()` command (note: you need to be
inside the `_system` database to execute this command):
    
    arangosh> db._createDatabase("mydb"); 
   
To retrieve the list of available databases, issue the following command from within
_arangosh_ (note: you need to be inside the `_system` database to execute this command):

    arangosh> db._listDatabases();

To switch to a different database, use the `db._useDatabase()` method:

    arangosh> db._useDatabase("mydb");

To remove an entire database with all collections, use the `db._dropDatabase()` method
(note: dropping a database can be executed from within the `_system` database):
    
    arangosh> db._dropDatabase("mydb");

Databases are dropped asynchronously when all clients have disconnected and all references
to the database have been garbage-collected. This is similar to how collections are
dropped.


Foxx applications are also active on a per-database level. Each database in an ArangoDB
instance can have different Foxx applications installed. To install a Foxx application
for a specific database, use the `--server.database` option when invoking the _foxx manager_
binary:

    unix> foxx-manager --server.database mydb update
    unix> foxx-manager --server.database mydb install hello-foxx /hello-foxx

Please note that when using the ArangoDB web interface, it will show the results of the
currently selected database only. This will in many cases be the `_system` database. 
Please check the @ref Upgrading14Databases "section on how to specify a database in the request"
to use the web interface with any other than the `_system` database.

Runtime Endpoint Management {#NewFeatures14Endpoints}
-----------------------------------------------------

The ArangoDB server can listen for incoming requests on multiple *endpoints*.

The endpoints are normally specified either in ArangoDB's configuration file or on
the command-line, using the @ref CommandLineArangoEndpoint "--server.endpoint" option.
In previous versions of ArangoDB, the endpoints could not be changed while the
server was running. 

In ArangoDB 1.4, the endpoints can also be changed at runtime. Each endpoint can
optionally be restricted to a specific database (or a list of databases). This endpoint-to-
database mapping allows the usage of different port numbers for different databases.  
This may be useful in multi-tenant setups. 

A multi-endpoint setup may also be useful to turn on encrypted communication for
just specific databases.

Asynchronous Execution {#NewFeatures14Async}
--------------------------------------------

ArangoDB 1.4 provides an additional mechanism for asynchronous request execution. Clients
can optionally mark any request as "to be executed asynchronously". Asynchronous execution 
decouples the execution from the actual server response, allowing clients to continue 
working earlier without waiting for the operation to finish.

Asynchronous requests are queued on the server and executed by the server as soon 
as possible. The maximum size of the queue can be bounded by using the startup option
`--scheduler.maximal-queue-size`. 

The results of asynchronously executed requests are either dropped immediately by the
server (i.e. _fire and forget_) or are stored in memory for later retrieval by the
client. ArangoDB provides an API for clients to retrieve the status of async jobs, and
to retrieve their responses.

Please refer to @ref HttpJob for more details. 

Dump and Reload Tools{#NewFeatures14DumpReload}
-----------------------------------------------

ArangoDB 1.4 comes with two tools for dumping collection data from an ArangoDB database
and reloading them later. These tools, _arangodump_ and _arangoreload_ can be used to
create and restore logical backups of an ArangoDB database.

_arangodump_ and _arangoreload_ are client tools that connect to a running ArangoDB
server. The dump tool will write the collection data into the file system in JSON format.
The restore tool can later be used to load the data created with the dump tool, either
into the same ArangoDB server instance, or into a different one.

The documentation for the tools can be found here: 
- @ref DumpManual
- @ref RestoreManual

Replication{#NewFeatures14Replication}
--------------------------------------

See @ref UserManualReplication for details.

Web Interface Improvements {#NewFeatures14WebInterface}
-------------------------------------------------------

The web interface now provides a graph viewer on the **Graphs** tab. The graph viewer
can be used to explore and navigate an existing ArangoDB graph. It supports both
graphs in the `_graphs` system collection as well as user-defined graphs that are
composed of an arbitrary vertex and edge collection.
Please note that when using ArangoDB's web interface with Internet Explorer
(IE), you will need IE version 9 or higher. The graph viewer relies on client-side 
SVG which is not available in previous versions of IE.

The **Dashboard** tab in the web interface provides an overview of server figures, which
can be adjusted to user needs. New figures are polled by the web interface in a 
configurable interval, and can be graphed as time-series.

The new **API** provides the full documentation of ArangoDB's built-in HTTP API. 
This allows browsing the ArangoDB API locally without the need to refer to the 
arangodb.org website for documentation.

The **Applications** tab in the web interface provides an improved overview of installed 
and available Foxx applications.

The **AQL Editor** now provides some example queries and allows saving user-defined queries
for later reuse.

The **Logs** tab in the web interface is now available in the `_system` database only.
This prevents the global server logs to be visible from within other than the `_system`
database.

The details view of collections in the web interface now shows more detailed figures 
and also a collection's available indexes. Indexes can be created and deleted directly
from the web interface now.

The web interface now also allows searching for documents based on arbitrary document
attribute values and has a document import / upload facility.

Journal File Handling Changes {#NewFeatures14Journals}
------------------------------------------------------

###Journal Creation
  
In ArangoDB versions prior to 1.4, a journal file was always created when a collection
was created. With the default journal size being 32 MB this has "wasted" a lot of disk 
space and RAM for collections that were created empty and did not contain any documents.

ArangoDB 1.4 delays the creation of a collection journal file until the first write in
the collection. If no journal is present when the first write is performed, a journal
is created on the fly. The same happens if a collection has no journal due to a previous
journal file rotation. The next write in the collection will create a new journal then.

This means that from now on there may be cases when a collection intentionally has no
journal. This should not affect end users except if they work on the collection files
directly with tools other than ArangoDB. And end users might benefit from ArangoDB
using less disk space for empty collections.

###Explicit Journal Rotation

Collections now also have a method named `rotate`, which can be called by clients to
explicitly close a collection's active journal. Explicitly closing a journal is not
necessary for normal operations, but as it turns the journal into a read-only datafile,
data from the journal becomes visible for the compaction. The compaction will always
ignore data in the current journal, and only data from datafiles will be compacted.
Explicitly closing the datafile and turning it into a journal can thus be used to
have data compacted if it is known that the last journal contained a lot of data that
has become superfluous due to later updates or deletions.

For example, calling `rotate` may make sense in the following situation:

    // create a collection and insert some data
    var test = db._create("test");
    for (var i = 0; i < 10000; ++i) {
      test.save({ _key: "test" + i, value: "abcdefg" }); 
    }

    ...
    // the following makes the collection empty, but "wasted" space will not yet be compacted
    test.truncate(); 

    // to make data in the journal become visible for the compaction, explicitly 
    // rotate the journal:
    test.rotate();
     
The actual gains that can be achieved by forcing rotation vary on journal size, document
sizes, performed operations and even the platform type, but in some situations the storage 
savings can be enormous as can be seen in the following example:

    var show = function (what, collection) { 
      var figures = collection.figures(); 
      require("internal").print(what, figures.datafiles.fileSize + figures.journals.fileSize);
    };

    var test = db._create("test");
    for (var i = 0; i < 10000; ++i) {
      test.save({ _key: "test" + i, value: "abcdefg" }); 
    }

    show("before truncate", test);
    test.truncate();
    show("after truncate", test);
    test.rotate();

    // wait for compaction
    require("internal").wait(30);
    show("after rotate", test);

The results for the above on some 64 bit test machine are:

    before truncate 33554432
    after truncate 33554432
    after rotate 184

Using `rotate` may also be useful when data in a collection is known to not change
in the immediate future. After having completed all write operations on a collection,
performing a `rotate` will reduce the size of the current journal to the actually
required size (journals are pre-allocated with a specific size when they are created) 
before making the journal a datafile. Thus `rotate` may cause disk space savings, even if
the datafiles does not qualify for being compacted after the rotation.

Journal rotation is executed asynchronously, meaning a call to `rotate` might return before
the rotation was executed.

The `rotate` function is also exposed via the collection REST API via PUT 
`/_api/collection/<name>/rotate`.

###Compaction

The collection compaction process in 1.4 may now merge smaller datafiles together. This
can reduce the number of file descriptors needed for a collection that has undergone a 
lot of compaction runs.

###Less Syncing during Creation

When collections are created, less sync system calls are executed when creating the
initial shapes for a collection. A collection is also created without a journal since 
1.4 if it is not populated instantly. This may reduce the overhead of creating a collection
in 1.4 compared to 1.3 and earlier.

AQL Improvements {#NewFeatures14Aql}
------------------------------------

The following functions have been added to AQL:

- `UNION_DISTINCT(list1, list2, ...)`: returns the union of distinct values of
  all lists specified. The function expects at least two list values as its arguments. 
  The result is a list of values in an undefined order.

- `MINUS(list1, list2, ...)`: returns the difference of all lists specified.
  The function expects at least two list values as its arguments.
  The result is a list of values that occur in the first list but not in any of the
  subsequent lists. The order of the result list is undefined and should not be relied on.
  Note: duplicates will be removed.

- `INTERSECTION(list1, list2, ...)`: returns the intersection of all lists specified.
  The function expects at least two list values as its arguments.
  The result is a list of values that occur in all arguments. The order of the result list
  is undefined and should not be relied on.
  Note: duplicates will be removed.

AQL query parse error messages now indicate the error position (line/column). If an
unknown collection is referenced in an AQL query, this is now more clearly stated in the
errors message.

AQL also has support for single-line comments now. Single-line comments are initiated
with a `//` and terminate automatically at line end.
 
AQL's `LIMIT` clause can now be used with bind parameters.

When executing an AQL query via an HTTP call (using the POST `/_api/cursor` API), it is
now possible to make the query return the number of documents before applying a LIMIT.
This can be achieved by providing the `fullCount` attribute in the `options` attribute.
See @ref HttpCursor for more details.

Collection Functions {#NewFeatures14Collections}
------------------------------------------------

###Cap Constraints

Cap constraints in ArangoDB can now be used to limit the number as well as the total size
of active documents in a collection. The size can be specified in bytes. This can be used
to keep a collection from growing endlessly.

The arguments for creating a cap constraint are now:

    collection.ensureCapConstraint(<count>, <byteSize>);

The following examples show how the constraints be employed in isolation or together:
    
    collection.ensureCapConstraint(100, 0);           // limit only number of documents
    collection.ensureCapConstraint(100);              // same

    collection.ensureCapConstraint(0, 1024 * 1024);   // limit only byte size
    collection.ensureCapConstraint(500, 1024 * 1024); // limit both count and byte size.

If two constraints are defined, the first met constraint will trigger the automatic 
deletion of "old" documents from the collection.

###Exists Function

ArangoDB 1.4 now provides `exists` methods on a collection and the database level. The
function can be used to quickly check if a document with a specific key or revision is
present.

###Checksum Function

ArangoDB provides a `checksum` method for collections. This method can be used to
calculate a hash value of the keys and optionally the document data in a collection. The
obtained hash value can be used to check 

- if data in a collection has changed over time: this can be achieved by calculating the 
  checksum of the collection at two different points in time and comparing them. 
  If nothing has changed, the two checksums should be identical).

- if two collections contain the same keys (and or revisions) and optionally document data

The `checksum` function is also exposed via the collection REST API at GET 
`/_api/collection/<name>/checksum`.

###First and Last Functions

ArangoDB 1.4 collections provide methods `first` and `last` to retrieve the _oldest_ and
_newest_ documents from a collection easily. The "age" of documents in a collection is
determined by document insertion or update time.

Using `first` and `last`, it is possible to use collections as a LIFO stacks or FIFO queues.

###Extended Figures

The `figures` method in ArangoDB 1.4 now also includes figures for the shape files owned
by a collection. These additional values can be used to better judge the actual disk space
usage of a collection. In pre-1.4 ArangoDB, the `figures` call did not include the size
of shape datafiles for a collection.

Miscellaneous Improvements {#NewFeatures14Misc}
-----------------------------------------------

ArangoDB 1.4 now provides a REST API to execute server-side traversals with custom 
traversal functions. The API is described @ref HttpTraversals "here".

The bulk import API now provides a `complete` URL parameter that can be used to
control the behaviour when at least one document cannot be imported. Setting
`complete` to `true` will abort the whole import and roll back any already imported
documents. Setting it to `false` or omitting it will make the import continue 
importing documents even if some documents could not be imported. This is also the
behaviour that previous ArangoDB versions exposed.

Command-Line Options added {#NewFeatures14Options}
--------------------------------------------------

Options can be specified on the command line or in configuration files. If a
string `@VARIABLE@` occurs in the value, it is replaced by the corresponding
environment variable.

The following command-line options have been added for _arangod_ in ArangoDB 1.4:

  * `--server.allow-method-override`: this option can be set to allow overriding the 
    HTTP request method in a request using one of the following custom headers:
    - x-http-method-override
    - x-http-method
    - x-method-override
    Using this option allows bypassing proxies and tools that would otherwise just 
    let certain types of requests pass. Enabling this option may impose a security 
    risk, so it should only be used in very controlled environments.
    The default value for this option is `false` (no method overriding allowed).

  * `--scheduler.maximal-queue-size`: limits the size of the asynchronous request
    execution queue. Please have a look at @ref NewFeatures14Async for more details.

  * `--server.authenticate-system-only`: require authentication only for requests 
    going to internal APIs, that is URLs starting with `/_` (e.g. `/_api/...`).

  * `--server.disable-replication-applier`: start the server with all replication 
    appliers disabled. This might be useful if you want to fix a replication setup problem.

  * `--server.disable-replication-logger`: start the server with all replication 
    loggers disabled. This might be useful if you want to fix a replication setup problem.
  
  * `--log.requests-file`: log incoming HTTP requests to a file

  * `--log.content-filter`: restrict log output to messages containing a specific
    string only. This is a debugging option.

  * `--log.source-filter` (renamed from `--log.filter`): restrict trace/debug log 
    output to messages originated by specific C/C++ source files. This is a debugging option.

 
The following command-line options have been added for _arangosh_ in 1.4:

  * `--server.database`: allows specifying the database name for the connection

  * `--prompt`: allows setting a user-defined prompt in arangosh. A `%d` in the
    prompt will be replaced with the name of the current database. 


The following command-line options have been added for _arangoimp_ in 1.4:

  * `--server.database`: allows specifying the database name for the connection

