Managing Users
==============

The user management in ArangoDB 3 is similar to the ones found in MySQL,
Postgres, or other database systems.

## Privileges

An ArangoDB server contains a list of users. It also defines various
privileges that can be assigned to a user. These privileges can be grouped
into three categories:

- server privileges
- database level privileges
- collection level privileges

The privileges on server level are

`create user`: allows to create a new user.

`update user`: allows to change the privileges and details of an existing
user.

`drop user`: allows to delete an existing user.

`create database`: allows to create a new database.

`drop database`: allows to delete an existing database.

The privileges on database level are tied to a given database and must be set
for each database individually. For a given database the privileges are

`create collection`: allows to create a new collection in the given database.

`update collection`: allows to update properties of an existing collection.

`drop collection`: allows to delete an existing collection.

`create index`: allows to create an index for an existing collection in the
given database.

`drop index`: allows to delete an index of an existing collection in the given
database.

The privileges on collection level are tied to a given collection of a given
database and must be set for each collection individually. For a given
collection the privileges are

`create document`: creates a new document in the given collection.

`read document`: read a document of the given collection.

`update document`: updates an existing document of the given collection.

`drop document`: deletes an existing document of the given collection.

`truncate collection`: deletes all documents of a given collection.

## Granting Privileges

In order to grant privileges to a user, you can assign on of three access
levels to the three categories.

For server level category privileges, an user needs the following access level
to gain a privilege. The access levels are *Administrate*, *Access* and
*No access*.

| action                | server level |
|-----------------------|--------------|
| create a database     | Administrate |
| drop a database       | Administrate |
| create a user         | Administrate |
| update a user         | Administrate |
| drop a user           | Administrate |

For database level category privileges, a user needs the following access
level to the given database. The access levels are *Administrate*, *Access*
and *No access*.

| action                | database level |
|-----------------------|----------------|
| create a collection   | Administrate   |
| drop a collection     | Administrate   |
| create an index       | Administrate   |
| drop an index         | Administrate   |

For collection level category privileges, an user needs the following acces
level to the given database and the given collection.  The access levels for
the database are *Administrate*, *Access* and *No access*.  The access levels
for the collection are *Read/Write*, *Read Only* and *No Access*.


| action                | collection level | database level         |
|-----------------------|------------------|------------------------|
| create a document     | Read/Write       | Administrate or Access |
| read a document       | Read Only        | Administrate or Access |
| update a document     | Read/Write       | Administrate or Access |
| drop a document       | Read/Write       | Administrate or Access |
| truncate a collection | Read/Write       | Administrate or Access |

### Example

For example, given

- a database *example*
- a collection *data* in the database *example*
- a user *doe*

If the user *doe* is assigned the access level *Access* for the database
*example* and the level *Read/Write* for the collection *data*, then the user
is allowed to create, read, update, deletes documents in the collection
*data*. But the user is, for example, not allowed to create indexes for the
collection *data* nor create new collections in the database *example*.

### Default Database Access Level

With the above definition, you must define the database access level for all
given and future databases in the server. In order to simplify this process,
it is possible to define a default database access level. This default is used
if database access level is *not* explicitly defined for a given database.

There exists only *one* default database access level. Changing the default
database access level will change the access level for all database that have
no explicitly defined access level.

### Default Collection Access Level

For each database there a default collection access level. This level is used
for all collection without an explicitly defined collection access level.

Unlike the default database access level, there exists a default collection
access level for each database.

## Managing Users in the Web Interface

In order to manage users use the web interface. Log into the *_system*
database and go to the "User" section. Select a user and go to the
*Permissions* tab. You will see a list of databases and their corresponding
database access level.

Please note, that server level privileges are defined as privileges on the
database *_system*.

If you click on a database, the list of collections will be open and you can
see the defined collection access levels for each collection of that database.

## Manging Users in The ArangoDB Sheel

Alternatively, you can use the ArangoDB shell. Fire up *arangosh* and require
the users module.

Please note, that for backward compatibility the server level privileges are
express as privileges on the database *_system*.

Also note, that the server and database access levels are represented as

* `rw`: for administrate
* `ro`: for access
* `none`: for no access

This is again for backward compatibility.

### Example

```
arangosh> var users = require('@arangodb/users');
arangosh> users.save('admin@testapp', 'mypassword');
```

Creates a user call *admin@testapp*. This user will have no access at all.

```
arangosh> users.grantDatabase('admin@testapp', 'testdb', 'rw');
```

This grants the user read write access to the database
*testdb*. `revokeDatabase` will revoke the right.

**Note**: Be aware that from 3.2 the `grantDatabase` will not automatically
grant users the right to write or read collections in a database.  If you
grant read only rights on a database `testdb` you will need to explicitly
grant access rights to individual collections via `grantCollection`.

```
arangosh> users.grantCollection('admin@testapp', 'testdb', 'testcoll', 'rw');
```

### Save

`users.save(user, passwd, active, extra)`

This will create a new ArangoDB user. The username must be specified in *user*
and must not be empty.

The password must be given as a string, too, but can be left empty if
required.  If you pass the special value *ARANGODB_DEFAULT_ROOT_PASSWORD*, the
password will be set the value stored in the environment variable
`ARANGODB_DEFAULT_ROOT_PASSWORD`. This can be used to pass an instance
variable into ArangoDB. For example, the instance identifier from Amazon.

If the *active* attribute is not specified, it defaults to *true*. The *extra*
attribute can be used to save custom data with the user.

This method will fail if either the username or the passwords are not
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

This grants *type* ('rw' or 'ro') access to the *database* for the *user*.

Server level privileges are expressed as privileges for the database *_system*.

### Revoke Database

`users.revokeDatabase(user, database)`

This revokes read/write access to the *database* for the *user*.

### Grant Collection

`users.grantCollection(user, database, collection, type)`

This grants *type* ('rw' or 'ro') access to the *collection* in *database* for
the *user*.

### Revoke Collection

`users.revokeCollection(user, database)`

This revokes read/write access to the *collection* for the *user*.

### Replace

`users.replace(user, passwd, active, extra)`

This will look up an existing ArangoDB user and replace its user data.

The username must be specified in *user*, and a user with the specified name
must already exist in the database.

The password must be given as a string, too, but can be left empty if required.

If the *active* attribute is not specified, it defaults to *true*.  The
*extra* attribute can be used to save custom data with the user.

This method will fail if either the username or the passwords are not specified
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

The username must be specified in *user* and the user must already exist in
the database.

The password must be given as a string, too, but can be left empty if required.

If the *active* attribute is not specified, the current value saved for the
user will not be changed. The same is true for the *extra* attribute.

This method will fail if either the username or the passwords are not specified
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

Checks whether the given combination of username and password is valid.  The
function will return a boolean value if the combination of username and password
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

The username must be specified in *User* and the specified user must exist in
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

The username must be specified in *user*.

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
automatically, and this can be performed by called this method.

*Examples*

    @startDocuBlockInline USER_03_reloadUser
    @EXAMPLE_ARANGOSH_OUTPUT{USER_03_reloadUser}
    require("@arangodb/users").reload();
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock USER_03_reloadUser
