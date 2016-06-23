!CHAPTER Managing Users

The user management in ArangoDB 3 is similar to the one found in
MySQL, Postgres, or other database systems.

An ArangoDB server contains a list of users. Each user can have access
to one or more databases (or none for that matter).

In order to manage users use the web interface. Log into the *_system*
database and go to the "User" section.

!SECTION Using the ArangoDB shell

Alternatively, you can use the ArangoDB shell. Fire up *arangosh*
and require the users module.

```
arangosh> var users = require("@arangodb/users");
arangosh> users.save("admin@testapp", "mypassword");
```

Creates an user call *admin@testapp*. This user will have no access
at all.

```
arangosh> users.grantDatabase("admin@testapp", "testdb");
```

This grants the user access to the database *testdb*. `revokeDatabase`
will revoke the right.

!SUBSECTION Save

`users.save(user, passwd, active, extra)`

This will create a new ArangoDB user. The username must be specified in
*user* and must not be empty.

The password must be given as a string, too, but can be left empty if required.
If you pass the special value *ARANGODB_DEFAULT_ROOT_PASSWORD*, the password
will be set the value stored in the environment variable
`ARANGODB_DEFAULT_ROOT_PASSWORD`. This can be used to pass an instance variable
into ArangoDB. For example, the instance identifier from Amazon.

If the *active* attribute is not specified, it defaults to *true*. The
*extra* attribute can be used to save custom data with the user.

This method will fail if either the username or the passwords are not specified
or given in a wrong format, or there already exists a user with the specified
name.

**Note**: the user will not have permission to access any database. You need to
grant the access rights for one or more databases using
[grantDatabase](#grant-database).

*Examples*

    @startDocuBlockInline USER_02_saveUser
    @EXAMPLE_ARANGOSH_OUTPUT{USER_02_saveUser}
    require("@arangodb/users").save("my-user", "my-secret-password");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock USER_02_saveUser

!SUBSECTION Grant Database

`users.grantDatabase(user, database)`

This grants read/write access to the *database* for the *user*.

If a user has access rights to the *_system* database, he is considered superuser.

!SUBSECTION Revoke Database

`users.revokeDatabase(user, database)`

This revokes read/write access to the *database* for the *user*.

!SUBSECTION Replace

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

!SUBSECTION Update

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

!SUBSECTION isValid

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

!SUBSECTION Remove

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

!SUBSECTION Document

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

!SUBSECTION all()

`users.all()`

Fetches all existing ArangoDB users from the database.

*Examples*

    @startDocuBlockInline USER_06_AllUsers
    @EXAMPLE_ARANGOSH_OUTPUT{USER_06_AllUsers}
    require("@arangodb/users").all();
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock USER_06_AllUsers

!SUBSECTION Reload

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

!SECTION Comparison to ArangoDB 2

ArangoDB 2 contained separate users per database. It was not possible
to give an user access to two or more databases. This proved
impractical.  Therefore we switch to a more common user model in
ArangoDB 3.

!SUBSECTION Command-Line Options for the Authentication and Authorization

<!-- arangod/RestServer/ArangoServer.h -->
@startDocuBlock server_authentication

<!-- arangod/RestServer/ArangoServer.h -->
@startDocuBlock serverAuthenticateSystemOnly
