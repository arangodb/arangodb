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
can only be carried out from within the `_system` database.

All command-line tools (e.g. _arangosh_, _arangoimp_) will connect to the `_system`
database by default. They also provide the option `--server.database` to connect to a
different database, e.g.:

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


@copydoc GlossaryDatabaseName

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
`-scheduler.maximal-queue-size`. 

The results of asynchronously executed requests are either dropped immediately by the
server (i.e. _fire and forget_) or are stored in memory for later retrieval by the
client. ArangoDB provides an API for clients to retrieve the status of async jobs, and
to retrieve their responses.

Please refer to @ref HttpJob for more details. 

Replication{#NewFeatures14Replication}
--------------------------------------

See @ref UserManualReplication for details.
