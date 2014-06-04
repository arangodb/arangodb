<a name="working_with_databases"></a>
# Working with Databases

<a name="database_methods"></a>
### Database Methods

The following methods are available to manage databases via JavaScript.
Please note that several of these methods can be used from the _system
database only.

`db._name()`

Returns the name of the current database as a string.

`db._id()`

Returns the id of the current database as a string.

`db._path()`
Returns the filesystem path of the current database as a string.

`db._isSystem()`

Returns whether the currently used database is the _system database. The system database has some special privileges and properties, for example, database management operations such as create or drop can only be executed from within this database. Additionally, the _system database itself cannot be dropped.

`db._useDatabase( name)`

Changes the current database to the database specified by name. Note that the database specified by name must already exist.

Changing the database might be disallowed in some contexts, for example server-side actions (including Foxx).

When performing this command from arangosh, the current credentials (username and password) will be re-used. These credentials might not be valid to connect to the database specified by name. Additionally, the database only be accessed from certain endpoints only. In this case, switching the database might not work, and the connection / session should be closed and restarted with different username and password credentials and/or endpoint data.

`db._listDatabases()`

Returns the list of all databases. This method can only be used from within the _system database.

`db._createDatabase( name, options, users)`

Creates a new database with the name specified by name. There are restrictions for database names ([see Database Names](../NamingConventions/DatabaseNames.md)).

Note that even if the database is created successfully, there will be no change into the current database to the new database. Changing the current database must explicitly be requested by using the db._useDatabase method.

The optional users attribute can be used to create initial users for the new database. If specified, it must be a list of user objects. Each user object can contain the following attributes:

* username: the user name as a string. This attribute is mandatory.
* passwd: the user password as a string. If not specified, then it defaults to the empty string.
* active: a boolean flag indicating whether the user accout should be actived or not. The default value is true.
* extra: an optional JSON object with extra user information. The data contained in extra will be stored for the user but not be interpreted further by ArangoDB.

If no initial users are specified, a default user root will be created with an empty string password. This ensures that the new database will be accessible via HTTP after it is created.

This method can only be used from within the _system database.

`db._dropDatabase( name)`

Drops the database specified by name. The database specified by name must exist.

Note that dropping databases is only possible from within the _system database. The _system database itself cannot be dropped.

Databases are dropped asynchronously, and will be physically removed if all clients have disconnected and references have been garbage-collected.




<!--
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
-->