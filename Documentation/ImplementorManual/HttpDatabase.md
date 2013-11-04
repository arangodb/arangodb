HTTP Interface for Databases {#HttpDatabase}
============================================

@NAVIGATE_HttpDatabase
@EMBEDTOC{HttpDatabaseTOC}

Address of a Database {#HttpDatabaseAddress}
============================================

Any operation triggered via ArangoDB's HTTP REST API is executed in the context of exactly
one database. To explicitly specify the database in a request, the request URI must contain
the database name in front of the actual path:

    http://localhost:8529/_db/mydb/...

where `...` is the actual path to the accessed resource. In the example, the resource will be
accessed in the context of the database `mydb`. Actual URLs in the context of `mydb` could look
like this:

    http://localhost:8529/_db/mydb/_api/version
    http://localhost:8529/_db/mydb/_api/document/test/12345
    http://localhost:8529/_db/mydb/myapp/get

Database-to-Endpoint Mapping {#HttpDatabaseMapping}
===================================================

If a database name is present in the URI as above, ArangoDB will consult the database-to-endpoint
mapping for the current endpoint, and validate if access to the database is allowed on the 
endpoint. 
If the endpoint is not restricted to a list of databases, ArangoDB will continue with the 
regular authentication procedure. If the endpoint is restricted to a list of specified databases,
ArangoDB will check if the requested database is in the list. If not, the request will be turned
down instantly. If yes, then ArangoDB will continue with the regular authentication procedure.

If the request URI was `http://localhost:8529/_db/mydb/...`, then the request to `mydb` will be 
allowed (or disallowed) in the following situations: 

    Endpoint-to-database mapping           Access to `mydb` allowed?
    ----------------------------           -------------------------
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

Database Management {#HttpDatabaseManagement}
=============================================

This is an introduction to ArangoDB's Http interface for managing databases.

The HTTP interface for databases provides operations to create and drop
individual databases. These are mapped to the standard HTTP methods `POST`
and `DELETE`. There is also the `GET` method to retrieve a list of existing
databases.

Please note that all database management operations can only be accessed via
the default database (`_system`) and none of the other databases.

Managing Databases using HTTP {#HttpDatabaseHttp}
=================================================

@anchor HttpDatabaseCurrent
@copydetails JSF_get_api_database_current

@CLEARPAGE
@anchor HttpDatabaseUser
@copydetails JSF_get_api_database_user

@CLEARPAGE
@anchor HttpDatabaseList
@copydetails JSF_get_api_database_list

@CLEARPAGE
@anchor HttpDatabaseCreate
@copydetails JSF_post_api_database

@CLEARPAGE
@anchor HttpDatabaseDelete
@copydetails JSF_delete_api_database

@CLEARPAGE
Notes on Databases {#HttpDatabaseGlossary}
==========================================

Please keep in mind that each database contains its own system collections,
which need to set up when a database is created. This will make the creation
of a database take a while. Replication is configured on a per-database level,
meaning that any replication logging or applying for the a new database must
be configured explicitly after a new database has been created. Foxx applications
are also available only in the context of the database they have been installed 
in. A new database will only provide access to the system applications shipped
with ArangoDB (that is the web interface at the moment) and no other Foxx
applications until they are explicitly installed for the particular database.

@copydoc GlossaryDatabase

@copydoc GlossaryDatabaseName

@copydoc GlossaryDatabaseOrganisation
