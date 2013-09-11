Handling Databases {#HandlingDatabases}
=======================================

@NAVIGATE_HandlingDatabases
@EMBEDTOC{HandlingDatabasesTOC}

JavaScript Interface to Databases {#HandlingDatabasesIntro}
===========================================================

This is an introduction to managing databases in ArangoDB from within 
JavaScript. By default (i.e. if not specified otherwise), ArangoDB
will use the default database (`_system`) for all incoming connections,
including connections from arangosh.

While being in an established connection to ArangoDB, the current
database can be changed explicity by using the `db._useDatabase()`
method. This will switch to the specified database (provided it
exists and the user can connect to it). From this point on, any
following actions in the same shell or connection will use the
specified database unless otherwise specified.

Connecting to a specific database from arangosh is possible with
the above command after arangosh has been started, but it is also
possible to specify a database name when invoking arangosh. 
For this purpose, use the command-line parameter `--server.database`,
e.g.

    arangosh --server.database test 

Please note that no commands, actions, or scripts should ever
access multiple databases, even if they exist. The only intended and
supported way in ArangoDB is to have a command, an action or a 
script use one database at a time.

@copydoc GlossaryDatabase

@copydoc GlossaryDatabaseName

@CLEARPAGE
Working with Databases {#HandlingDatabasesShell}
================================================

Database Methods {#HandlingDatabasesMethods}
--------------------------------------------

@anchor HandlingDatabasesList
@copydetails JS_ListDatabases

@CLEARPAGE
@anchor HandlingDatabasesCreate
@copydetails JS_CreateDatabase

@CLEARPAGE
@anchor HandlingDatabasesUse
@copydetails JS_UseDatabase

@CLEARPAGE
@anchor HandlingDatabasesDrop
@copydetails JS_DropDatabase

@CLEARPAGE
@anchor HandlingDatabasesName
@copydetails JS_NameDatabase

