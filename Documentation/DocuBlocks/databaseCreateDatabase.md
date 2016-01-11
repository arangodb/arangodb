

@brief create a new database
`db._createDatabase(name, options, users)`

Creates a new database with the name specified by *name*.
There are restrictions for database names
(see [DatabaseNames](../NamingConventions/DatabaseNames.md)).

Note that even if the database is created successfully, there will be no
change into the current database to the new database. Changing the current
database must explicitly be requested by using the
*db._useDatabase* method.

The *options* attribute currently has no meaning and is reserved for
future use.

The optional *users* attribute can be used to create initial users for
the new database. If specified, it must be a list of user objects. Each user
object can contain the following attributes:

* *username*: the user name as a string. This attribute is mandatory.
* *passwd*: the user password as a string. If not specified, then it defaults
  to the empty string.
* *active*: a boolean flag indicating whether the user account should be
  active or not. The default value is *true*.
* *extra*: an optional JSON object with extra user information. The data
  contained in *extra* will be stored for the user but not be interpreted
  further by ArangoDB.

If no initial users are specified, a default user *root* will be created
with an empty string password. This ensures that the new database will be
accessible via HTTP after it is created.

You can create users in a database if no initial user is specified. Switch 
into the new database (username and password must be identical to the current
session) and add or modify users with the following commands.

```js
  require("@arangodb/users").save(username, password, true);
  require("@arangodb/users").update(username, password, true);
  require("@arangodb/users").remove(username);
```
Alternatively, you can specify user data directly. For example:

```js
  db._createDatabase("newDB", [], [{ username: "newUser", passwd: "123456", active: true}])
```

Those methods can only be used from within the *_system* database.

