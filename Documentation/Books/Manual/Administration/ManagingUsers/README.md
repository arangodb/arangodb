Managing Users
==============

The user management in ArangoDB 3 is similar to the ones found in MySQL,
PostgreSQL, or other database systems.

## Actions and Access Levels

An ArangoDB server contains a list of users. It also defines various
access levels that can be assigned to a user (for details, see below)
and that are needed to perform certain actions. These actions can be grouped
into three categories:

- server actions
- database actions
- collection actions

The server actions are

`create user`: allows to create a new user.

`update user`: allows to change the access levels and details of an existing
user.

`drop user`: allows to delete an existing user.

`create database`: allows to create a new database.

`drop database`: allows to delete an existing database.

The database actions are tied to a given database, and access
levels must be set
for each database individually. For a given database the actions are

`create collection`: allows to create a new collection in the given database.

`update collection`: allows to update properties of an existing collection.

`drop collection`: allows to delete an existing collection.

`create index`: allows to create an index for an existing collection in the
given database.

`drop index`: allows to delete an index of an existing collection in the given
database.

The collection actions are tied to a given collection of a given
database, and access levels must be set for each collection individually.
For a given collection the actions are

`read document`: read a document of the given collection.

`create document`: creates a new document in the given collection.

`modify document`: modifies an existing document of the given collection,
this can be an update or replace operation.

`drop document`: deletes an existing document of the given collection.

`truncate collection`: deletes all documents of a given collection.

For the server category, a user needs the following access level
to perform an action. The access levels are *Administrate* and
*No access*:

| action                    | server level |
|---------------------------|--------------|
| create a database         | Administrate |
| drop a database           | Administrate |
| create a user             | Administrate |
| update a user             | Administrate |
| update user access level  | Administrate |
| drop a user               | Administrate |

For the database category, a user needs the following access
level to the given database to perform an action. The access levels
are *Administrate*, *Access* and *No access*.

| action                | database level |
|-----------------------|----------------|
| create a collection   | Administrate   |
| update a collection   | Administrate   |
| drop a collection     | Administrate   |
| create an index       | Administrate   |
| drop an index         | Administrate   |

Note that the access level *Access* for a database is relevant for the
collection category below!

For the collection category, a user needs the following access
levels to the given database and the given collection. The access levels for
the database are *Administrate*, *Access* and *No access*. The access levels
for the collection are *Read/Write*, *Read Only* and *No Access*.


| action                | collection level        | database level         |
|-----------------------|-------------------------|------------------------|
| read a document       | Read/Write or Read Only | Administrate or Access |
| create a document     | Read/Write              | Administrate or Access |
| modify a document     | Read/Write              | Administrate or Access |
| drop a document       | Read/Write              | Administrate or Access |
| truncate a collection | Read/Write              | Administrate or Access |


*Example*

For example, given

- a database *example*
- a collection *data* in the database *example*
- a user *doe*

If the user *doe* is assigned the access level *Access* for the database
*example* and the level *Read/Write* for the collection *data*, then the user
is allowed to read, create, modify or delete documents in the collection
*data*. But the user is, for example, not allowed to create indexes for the
collection *data* nor create new collections in the database *example*.

## Granting Access Levels

In order to grant an access level to a user, you can assign one of
three access levels for each database and one of three levels for each
collection in a database. The server access level for the user follows
from the database access level in the `_system` database, it is
*Administrate* if and only if the database access level is
*Administrate*. Note that this means that database access level
*Access* does not grant a user server access level *Administrate*.

### Default Database Access Level

With the above definition, one must define the database access level for
all database/user pairs in the server, which would be very tedious. In
order to simplify this process, it is possible to define, for a user,
a default database access level. This default is used if the database
access level is *not* explicitly defined for a certain database.

Changing the default database access level for a user will change the
access level for all databases that have no explicitly defined
access level. Note that this includes databases which will be created
in the future and for which no explicit access levels are set for that
user!

If you delete the default, it is handled as if *No Access* was defined.

*Example*

Assume user *doe* has the following database access levels:

|                 | access level |
|-----------------|--------------|
|database "\*"    | Access       |
|database "snake" | Administrate |
|database "oil"   | No Access    |

This will give the user *doe* the following database level access:

- database "snake": *Administrate*
- database "oil": *No Access*
- database "company": *Access*

If the default "\*" is changed from *Access* to *No Access* then the
permissions will change as follows:

- database "snake": *Administrate*
- database "oil": *No Access*
- database "company": *No Access*

### Default Collection Access Level

For each user and database there is a default collection access level.
This level is used for all collections pairs without an explicitly
defined collection access level. Note that this includes collections
which will be created in the future and for which no explicit access
levels are set for a that user!

If you delete the default, it is handled as if *No Access* was defined.

*Example*

Assume user *doe* has the following database access levels:

|                 | access level |
|-----------------|--------------|
|database "\*"    | Access       |

and collection access levels:

|                                        | access level |
|----------------------------------------|--------------|
| database "\*", collection "\*"         | Read/Write   |
| database "snake", collection "company" | Read-Only    |
| database "snake", collection "\*"      | No Access    |
| database "oil", collection "\*"        | Read-Only    |

Then the user *doe* will get the following collection access levels:

- database "snake", collection "company": *Read-Only*
- database "snake", collection "potion": *No Access*
- database "oil", collection "vial": *Read-Only*
- database "something", collection "else": *Read/Write*

Explanation:

Database "snake", collection "company" directly matches a defined
access level. This level is defined as *Read-Only*.

Database "snake", collection "potion" does not match a defined access
level. However, database "snake" matches and the default in this
database for collection level is *No Access*.

Database "oil", collection "vial" does not match a defined access
level. However, database "oil" matches and the default in this
database for collection level is *Read-Only*.

Database "somehing", collection "else" does not match a defined access
level. The database "something" also does have a direct matches.
Therefore the default database is selected. The level is *Read/Write*.

### System Collections

The access level for system collections cannot be changed.

No user has access to the *_users* collection in the *_system*
database. All changes to the access levels must be done using the
*@arangodb/users* module.

All user have *Read/Write* access to the *\_frontend* collection in
databases they have either *Access* or *Administrate* access level.

All other system collections have access level *Read/Write* if the
user has *Administrate* access to the database. They have access level
*Read/Only* if the user has *Access* to the database.

## Managing Users in the Web Interface

In order to manage users use the web interface. Log into the *_system*
database and go to the "Users" section. Select a user and go to the
*Access levels* tab. You will see a list of databases and their
corresponding database access level for that user.

Please note that server access level follows from the access level on
the database *_system*. Furthermore, the default database access level
for this user appear in the artificial row with the database name `*`.

Below this table is another one for the collection category access
levels. At first, it shows the list of databases, too. If you click on a
database, the list of collections in that database will be open and you
can see the defined collection access levels for each collection of that
database (which can be all unselected which means that nothing is
explicitly set). The default access levels for this user and database
appear in the artificial row with the collection name `*`.

## Managing Users in The ArangoDB Shell

Alternatively, you can use the ArangoDB shell. Fire up *arangosh* and require
the users module.

Please note, that for backward compatibility the server access levels
follow from the database access level on the database *_system*.

Also note that the server and database access levels are represented as

* `rw`: for *Administrate*
* `ro`: for *Access*
* `none`: for *No access*

This is again for backward compatibility.

### Example

```
arangosh> var users = require('@arangodb/users');
arangosh> users.save('admin@testapp', 'mypassword');
```

Creates a user called *admin@testapp*. This user will have no access at all.

```
arangosh> users.grantDatabase('admin@testapp', 'testdb', 'rw');
```

This grants the user *Administrate* access to the database
*testdb*. `revokeDatabase` will revoke this access level setting.

**Note**: Be aware that from 3.2 onwards the `grantDatabase` will not
automatically grant users the access level to write or read collections in a
database. If you grant access to a database `testdb` you will
additionally need to explicitly grant access levels to individual
collections via `grantCollection`.

The upgrade procedure from 3.1 to 3.2 sets the default database access
level for all users to *Administrate* and sets the default collection
access level for all user/database pairs to *Read/Write*.

```
arangosh> users.grantCollection('admin@testapp', 'testdb', 'testcoll', 'rw');
```

### Save

`users.save(user, passwd, active, extra)`

This will create a new ArangoDB user. The user name must be specified in *user*
and must not be empty.

The password must be given as a string, too, but can be left empty if
required.  If you pass the special value *ARANGODB_DEFAULT_ROOT_PASSWORD*, the
password will be set the value stored in the environment variable
`ARANGODB_DEFAULT_ROOT_PASSWORD`. This can be used to pass an instance
variable into ArangoDB. For example, the instance identifier from Amazon.

If the *active* attribute is not specified, it defaults to *true*. The *extra*
attribute can be used to save custom data with the user.

This method will fail if either the user name or the passwords are not
specified or given in a wrong format, or there already exists a user with the
specified name.

**Note**: The user will not have permission to access any database. You need
to grant the access rights for one or more databases using
[grantDatabase](#grant-database).

*Examples*

    @startDocuBlockInline USER_02_saveUser
    @EXAMPLE_ARANGOSH_OUTPUT{USER_02_saveUser}
    require('@arangodb/users').save('my-user', 'my-secret-password');
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock USER_02_saveUser

### Grant Database

`users.grantDatabase(user, database, type)`

This grants *type* ('rw', 'ro' or 'none') access to the *database* for
the *user*. If *database* is `"*"`, this sets the default database access
level for the user *user*.

The server access level follows from the access level for the database
`_system`.

### Revoke Database

`users.revokeDatabase(user, database)`

This revokes the access level setting to the *database* for the *user*,
the default database access setting for this user kicks in.

### Grant Collection

`users.grantCollection(user, database, collection, type)`

This grants *type* ('rw', 'ro' or 'none') access level to the *collection*
in *database* for the *user*. If *collection* is `"*"` this sets the
default collection access level for the user *user* in database
*database*.

### Revoke Collection

`users.revokeCollection(user, database)`

This revokes the access level setting to the collection *collection* for the
user *user*.

### Replace

`users.replace(user, passwd, active, extra)`

This will look up an existing ArangoDB user and replace its user data.

The username must be specified in *user*, and a user with the specified name
must already exist in the database.

The password must be given as a string, too, but can be left empty if required.

If the *active* attribute is not specified, it defaults to *true*.  The
*extra* attribute can be used to save custom data with the user.

This method will fail if either the user name or the passwords are not specified
or given in a wrong format, or if the specified user cannot be found in the
database.

**Note**: this function will not work from within the web interface

*Examples*

    @startDocuBlockInline USER_03_replaceUser
    @EXAMPLE_ARANGOSH_OUTPUT{USER_03_replaceUser}
    require("@arangodb/users").replace("my-user", "my-changed-password");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock USER_03_replaceUser

### Update

`users.update(user, passwd, active, extra)`

This will update an existing ArangoDB user with a new password and other data.

The user name must be specified in *user* and the user must already exist in
the database.

The password must be given as a string, too, but can be left empty if required.

If the *active* attribute is not specified, the current value saved for the
user will not be changed. The same is true for the *extra* attribute.

This method will fail if either the user name or the passwords are not specified
or given in a wrong format, or if the specified user cannot be found in the
database.

*Examples*

    @startDocuBlockInline USER_04_updateUser
    @EXAMPLE_ARANGOSH_OUTPUT{USER_04_updateUser}
    require("@arangodb/users").update("my-user", "my-secret-password");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock USER_04_updateUser

### isValid

`users.isValid(user, password)`

Checks whether the given combination of user name and password is valid.  The
function will return a boolean value if the combination of user name and password
is valid.

Each call to this function is penalized by the server sleeping a random
amount of time.

*Examples*

    @startDocuBlockInline USER_05_isValidUser
    @EXAMPLE_ARANGOSH_OUTPUT{USER_05_isValidUser}
    require("@arangodb/users").isValid("my-user", "my-secret-password");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock USER_05_isValidUser

### Remove

`users.remove(user)`

Removes an existing ArangoDB user from the database.

The user name must be specified in *User* and the specified user must exist in
the database.

This method will fail if the user cannot be found in the database.

*Examples*

    @startDocuBlockInline USER_07_removeUser
    @EXAMPLE_ARANGOSH_OUTPUT{USER_07_removeUser}
    require("@arangodb/users").remove("my-user");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock USER_07_removeUser

### Document

`users.document(user)`

Fetches an existing ArangoDB user from the database.

The user name must be specified in *user*.

This method will fail if the user cannot be found in the database.

*Examples*

    @startDocuBlockInline USER_04_documentUser
    @EXAMPLE_ARANGOSH_OUTPUT{USER_04_documentUser}
    require("@arangodb/users").document("my-user");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock USER_04_documentUser

### all()

`users.all()`

Fetches all existing ArangoDB users from the database.

*Examples*

    @startDocuBlockInline USER_06_AllUsers
    @EXAMPLE_ARANGOSH_OUTPUT{USER_06_AllUsers}
    require("@arangodb/users").all();
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock USER_06_AllUsers

### Reload

`users.reload()`

Reloads the user authentication data on the server

All user authentication data is loaded by the server once on startup only and is
cached after that. When users get added or deleted, a cache flush is done
automatically, and this can be performed by a call to this method.

*Examples*

    @startDocuBlockInline USER_03_reloadUser
    @EXAMPLE_ARANGOSH_OUTPUT{USER_03_reloadUser}
    require("@arangodb/users").reload();
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock USER_03_reloadUser
