Handling Databases {#HandlingDatabases}
=======================================

@NAVIGATE_HandlingDatabases
@EMBEDTOC{HandlingDatabasesTOC}

JavaScript Interface to Databases {#HandlingDatabasesIntro}
===========================================================

This is an introduction to managing databases in ArangoDB from within 
JavaScript. 

While being in an established connection to ArangoDB, the current
database can be changed explicity by using the `db._useDatabase()`
method. This will switch to the specified database (provided it
exists and the user can connect to it). From this point on, any
following actions in the same shell or connection will use the
specified database unless otherwise specified.

Note: if the database is changed, client drivers need to store the 
current database name on their side, too. This is because connections
in ArangoDB do not contain any state information. All state information
is contained in the HTTP request/response data.

Connecting to a specific database from arangosh is possible with
the above command after arangosh has been started, but it is also
possible to specify a database name when invoking arangosh. 
For this purpose, use the command-line parameter `--server.database`,
e.g.

    > arangosh --server.database test 

Please note that commands, actions, scripts or AQL queries should never
access multiple databases, even if they exist. The only intended and
supported way in ArangoDB is to use one database at a time for a command,
an action, a script or a query. Operations started in one database must
not switch the database later and continue operating in another.

@copydoc GlossaryDatabase

@copydoc GlossaryDatabaseName

@CLEARPAGE
Working with Databases {#HandlingDatabasesShell}
================================================

Database Methods {#HandlingDatabasesMethods}
--------------------------------------------

To following methods are available to manage databases via JavaScript.
Please note that several of these methods can be used from the `_system`
database only.

@anchor HandlingDatabasesName
@copydetails JS_NameDatabase

@CLEARPAGE
@anchor HandlingDatabasesId
@copydetails JS_IdDatabase

@CLEARPAGE
@anchor HandlingDatabasesPath
@copydetails JS_PathDatabase

@CLEARPAGE
@anchor HandlingDatabasesIsSystem
@copydetails JS_IsSystemDatabase

@CLEARPAGE
@anchor HandlingDatabasesUse
@copydetails JS_UseDatabase

@CLEARPAGE
@anchor HandlingDatabasesList
@copydetails JS_ListDatabases

@CLEARPAGE
@anchor HandlingDatabasesCreate
@copydetails JS_CreateDatabase

@CLEARPAGE
@anchor HandlingDatabasesDrop
@copydetails JS_DropDatabase

