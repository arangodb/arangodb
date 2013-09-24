New Features in ArangoDB 1.4 {#NewFeatures14}
=============================================

@NAVIGATE_NewFeatures14
@EMBEDTOC{NewFeatures14TOC}

Features and Improvements {#NewFeatures14Introduction}
======================================================

The following list shows in detail which features have been added or improved in
ArangoDB 1.4.  ArangoDB 1.4 also contains several bugfixes that are not listed
here - see the CHANGELOG for details.

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


Any operation triggered via ArangoDB's HTTP REST API is executed in the context of exactly
one database. To explicitly specify the database in the request, the request URI must contain
the database name in front of the actual path:

    http://localhost:8529/_db/mydb/...

where `...` is the actual path to the accessed resource. In the example, the resource will be
accessed in the context of the database `mydb`. Actual URLs in the context of `mydb` could look
like this:

    http://localhost:8529/_db/mydb/_api/version
    http://localhost:8529/_db/mydb/_api/document/test/12345
    http://localhost:8529/_db/mydb/myapp/get

If a database name is present in the URI as above, ArangoDB will consult the database-to-endpoint
mapping for the current endpoint, and validate if access to the database is allowed on the 
endpoint. 
If the endpoint is not restricted to a list of databases, ArangoDB will continue with the 
regular authentication procedure. If the endpoint is restricted to a list of specified databases,
ArangoDB will check if the requested database is in the list. If not, the request will be turned
down instantly. If yes, then ArangoDB will continue with the regular authentication procedure.

If the request URI was `http://localhost:8529/_db/mydb/...`, then the request to `mydb` will be 
allowed (or disallowed) in the following situations: 

    Endpoint-to-database mapping           Access to `mydb` allowed (subject to further authentication)
    ----------------------------           ------------------------------------------------------------
    [ ]                                    yes
    [ "_system" ]                          no 
    [ "_system", "mydb" ]                  yes
    [ "mydb" ]                             yes
    [ "mydb", "_system" ]                  yes
    [ "test1", "test2" ]                   no

In case no database name is specified in the request URI, ArangoDB will derive the database
name from the endpoint-to-database mapping (see @ref NewFeatures14Endpoints} of the endpoint 
the connection was coming in on. 

If the endpoint is not restricted to a list of databases, ArangoDB will assume the `_system`
database. If the endpoint is restricted to one or multiple databases, ArangoDB will assume
the first name from the list.

Following is an overview of which database name will be assumed for different endpoint-to-database
mappings in case no database name is specified in the URI:

    Endpoint-to-database mapping           Database
    ----------------------------           --------
    [ ]                                    _system
    [ "_system" ]                          _system
    [ "_system", "mydb" ]                  _system
    [ "mydb" ]                             mydb
    [ "mydb", "_system" ]                  mydb


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

ArangoDB is a multi-threaded server, allowing the processing of multiple client 
requests at the same time. Communication handling and the actual work can be performed
by multiple worker threads in parallel.

Though multiple clients can connect and send their requests in parallel to ArangoDB,
clients may need to wait for their requests to be processed.

By default, the server will fully process an incoming request and then return the
result to the client. The client must wait for the server's response before it can
send additional requests over the connection. For clients that are single-threaded
or not event-driven, waiting for the full server response may be non-optimal.

To mitigate client blocking issues, ArangoDB since version 1.4. offers a generic mechanism 
for non-blocking requests: if clients add the HTTP header `x-arango-async: true` to their
requests, ArangoDB will put the request into an in-memory task queue and return an HTTP 202
(accepted) response to the client instantly. The server will execute the tasks from
the queue asynchronously, decoupling the client requests and the actual work.

This allows for much higher throughput than if clients would wait for the server's
response. The downside is that the response that is sent to the client is always the
same (a generic HTTP 202) and clients cannot make a decision based on the actual
operation's result. In fact, the operation might have not even been executed at the
time the generic response has reached the client. Clients can thus not rely on their
requests having been processed successfully.

The asynchronous task queue on the server is not persisted, meaning not-yet processed
tasks from the queue might be lost in case of a crash.

Clients should thus not send the extra header when they have strict durability 
requirements or if they rely on result of the sent operation for further actions.


Replication{#NewFeatures14Replication}
--------------------------------------

See @ref UserManualReplication for details.
