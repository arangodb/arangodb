Authentication and Authorisation{#DbaManualAuthentication}
==========================================================

@NAVIGATE_DbaManualAuthentication
@EMBEDTOC{DbaManualAuthenticationTOC}

Authentication and Authorisation{#DbaManualAuthenticationIntro}
===============================================================

ArangoDB only provides a very simple authentication interface and no
authorisation. We plan to add authorisation features in later releases, which
will allow the administrator to restrict access to collections and queries to
certain users, given them either read or write access.

Currently, you can only secure the access to ArangoDB in an all-or-nothing
fashion. The collection `_users` contains all users and a salted SHA256 hash
of their passwords. A user can be active or inactive. A typical document of this
collection is

@EXAMPLE_ARANGOSH_OUTPUT{AuthenticationExample1}
    db._users.firstExample("user", "root")
@END_EXAMPLE_ARANGOSH_OUTPUT

Command-Line Options for the Authentication and Authorisation{#DbaManualAuthenticationCommandLine}
--------------------------------------------------------------------------------------------------

@copydetails triagens::arango::ArangoServer::_disableAuthentication

Introduction to User Management{#UserManagementIntro}
=====================================================

ArangoDB provides basic functionality to add, modify and remove database users
programmatically. The following functionality is provided by the `users` module
and can be used from inside arangosh and arangod.

Please note that this functionality is not available from within the web
interface.

@COMMENT{######################################################################}
@anchor UserManagementSave

@FUN{users.save(@FA{user}, @FA{passwd}, @FA{active}, @FA{extra}, @FA{changePassword})}

This will create a new ArangoDB user. The username must be specified in
@FA{user} and must not be empty.

The password must be given as a string, too, but can be left empty if required.

If the @FA{active} attribute is not specified, it defaults to `true`. The
@FA{extra} attribute can be used to save custom data with the user.

If the @FA{changePassword} attribute is not specified, it defaults to `false`.
The @FA{changePassword} attribute can be used to indicate that the user must
change has password before logging in.

This method will fail if either the username or the passwords are not specified
or given in a wrong format, or there already exists a user with the specified
name.

The new user account can only be used after the server is either restarted or
the server authentication cache is @ref UserManagementReload "reloaded".

Note: this function will not work from within the web interface

@EXAMPLES

    arangosh> require("org/arangodb/users").save("my-user", "my-secret-password");

@COMMENT{######################################################################}
@CLEARPAGE
@anchor UserManagementDocument
@FUN{users.document(@FA{user})}

Fetches an existing ArangoDB user from the database.

The username must be specified in @FA{user}.

This method will fail if the user cannot be found in the database.

Note: this function will not work from within the web interface

@COMMENT{######################################################################}
@CLEARPAGE
@anchor UserManagementReplace
@FUN{users.replace(@FA{user}, @FA{passwd}, @FA{active}, @FA{extra}, @FA{changePassword})}

This will look up an existing ArangoDB user and replace its user data.

The username must be specified in @FA{user}, and a user with the specified name
must already exist in the database.

The password must be given as a string, too, but can be left empty if required.

If the @FA{active} attribute is not specified, it defaults to `true`.  The
@FA{extra} attribute can be used to save custom data with the user.

If the @FA{changePassword} attribute is not specified, it defaults to `false`.
The @FA{changePassword} attribute can be used to indicate that the user must
change has password before logging in.

This method will fail if either the username or the passwords are not specified
or given in a wrong format, or if the specified user cannot be found in the
database.

Note: this function will not work from within the web interface

@EXAMPLES

    arangosh> require("org/arangodb/users").replace("my-user", "my-changed-password");

@COMMENT{######################################################################}
@CLEARPAGE
@anchor UserManagementUpdate
@FUN{@FA{users}.update(@FA{user}, @FA{passwd}, @FA{active}, @FA{extra}, @FA{changePassword})}

This will update an existing ArangoDB user with a new password and other data.

The username must be specified in @FA{user} and the user must already exist in
the database.

The password must be given as a string, too, but can be left empty if required.

If the @FA{active} attribute is not specified, the current value saved for the
user will not be changed. The same is true for the @FA{extra} and the
@FA{changePassword} attribute.

This method will fail if either the username or the passwords are not specified
or given in a wrong format, or if the specified user cannot be found in the
database.

Note: this function will not work from within the web interface

@EXAMPLES

    arangosh> require("org/arangodb/users").update("my-user", "my-secret-password");

@COMMENT{######################################################################}
@CLEARPAGE
@anchor UserManagementRemove
@FUN{users.remove(@FA{user})}

Removes an existing ArangoDB user from the database.

The username must be specified in @FA{user} and the specified user must exist in
the database.

This method will fail if the user cannot be found in the database.

Note: this function will not work from within the web interface

@EXAMPLES

    arangosh> require("org/arangodb/users").remove("my-user");

@COMMENT{######################################################################}
@CLEARPAGE
@anchor UserManagementReload
@FUN{users.reload()}

Reloads the user authentication data on the server

All user authentication data is loaded by the server once on startup only and is
cached after that. When users get added or deleted, a cache flush is required,
and this can be performed by called this method.

Note: this function will not work from within the web interface

@COMMENT{######################################################################}
@CLEARPAGE
@anchor UserManagementIsValid
@FUN{users.isvalid(@FA{user}, @FA{password})}

Checks whether the given combination of username and password is valid.  The
function will return a boolean value if the combination of username and password
is valid.

Each call to this function is penalized by the server sleeping a random
amount of time.

Note: this function will not work from within the web interface

@COMMENT{######################################################################}
@CLEARPAGE
@anchor UserManagementAll
@FUN{users.all()}

Fetches all existing ArangoDB users from the database.

@COMMENT{######################################################################}
@BNAVIGATE_DbaManualAuthentication
