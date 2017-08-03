Managing Users in the ArangoDB Shell
====================================

Please note, that for backward compatibility the server access levels
follow from the database access level on the database *_system*.

Also note that the server and database access levels are represented as

* `rw`: for *Administrate*
* `ro`: for *Access*
* `none`: for *No access*

This is again for backward compatibility.

### Example

Fire up *arangosh* and require the users module. Use it to create a new user:

```
arangosh> var users = require('@arangodb/users');
arangosh> users.save('JohnSmith', 'mypassword');
```

Creates a user called *JohnSmith*. This user will have no access at all.

```
arangosh> users.grantDatabase('JohnSmith', 'testdb', 'rw');
```

This grants the user *Administrate* access to the database
*testdb*. `revokeDatabase` will revoke this access level setting.

**Note**: Be aware that from 3.2 onwards the `grantDatabase` will not
automatically grant users the access level to write or read collections in a
database. If you grant access to a database `testdb` you will
additionally need to explicitly grant access levels to individual
collections via `grantCollection`.

The upgrade procedure from 3.1 to 3.2 sets the wildcard database access
level for all users to *Administrate* and sets the wildcard collection
access level for all user/database pairs to *Read/Write*.

```
arangosh> users.grantCollection('JohnSmith', 'testdb', 'testcoll', 'rw');
```

Save
----

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

Grant Database
--------------

`users.grantDatabase(user, database, type)`

This grants *type* ('rw', 'ro' or 'none') access to the *database* for
the *user*. If *database* is `"*"`, this sets the wildcard database access
level for the user *user*.

The server access level follows from the access level for the database
`_system`.

Revoke Database
---------------

`users.revokeDatabase(user, database)`

This clears the access level setting to the *database* for the *user* and
the wildcard database access setting for this user kicks in. In case no wildcard
access was defined the default is *No Access*. This will also
clear the access levels for all the collections in this database.

Grant Collection
----------------

`users.grantCollection(user, database, collection, type)`

This grants *type* ('rw', 'ro' or 'none') access level to the *collection*
in *database* for the *user*. If *collection* is `"*"` this sets the
wildcard collection access level for the user *user* in database
*database*.

Revoke Collection
-----------------

`users.revokeCollection(user, database)`

This clears the access level setting to the collection *collection* for the
user *user*. The system will either fallback to the wildcard collection access
level or default to *No Access*

Replace
-------

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

Update
------

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

isValid
-------

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

Remove
------

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

Document
--------

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

All
---

`users.all()`

Fetches all existing ArangoDB users from the database.

*Examples*

    @startDocuBlockInline USER_06_AllUsers
    @EXAMPLE_ARANGOSH_OUTPUT{USER_06_AllUsers}
    require("@arangodb/users").all();
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock USER_06_AllUsers

Reload
------

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


Permission
--------

`users.permission(user, database[, collection])`

Fetches the access level to the database or a collection.

The user and database name must be specified, optionally you can specify
the collection name.

This method will fail if the user cannot be found in the database.

*Examples*

    @startDocuBlockInline USER_05_permission
    @EXAMPLE_ARANGOSH_OUTPUT{USER_05_permission}
    ~ require("@arangodb/users").grantDatabase("my-user", "testdb");
    require("@arangodb/users").permission("my-user", "testdb");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock USER_05_permission


