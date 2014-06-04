<a name="authentication_and_authorization"></a>
# Authentication and Authorization

<a name="authentication_and_authorization"></a>
### Authentication and Authorization

ArangoDB only provides a very simple authentication interface and no
authorization. We plan to add authorization features in later releases, which
will allow the administrator to restrict access to collections and queries to
certain users, given them either read or write access.

Currently, you can only secure the access to ArangoDB in an all-or-nothing
fashion. The collection `_users` contains all users and a salted SHA256 hash
of their passwords. A user can be active or inactive. A typical document of this
collection is

	{ 
	  "_id" : "_users/1172449", 
	  "_rev" : "1172449", 
	  "_key" : "1172449", 
	  "active" : true, 
	  "changePassword" : false, 
	  "user" : "root", 
	  "password" : "$1$bd5458a8$8b23e2e1a762f75001ab182235b8ab1b8665bc572b0734a042a501b3c34e567a" 
	}

<!--

@EXAMPLE_ARANGOSH_OUTPUT{AuthenticationExample1}
    db._users.firstExample("user", "root")
@END_EXAMPLE_ARANGOSH_OUTPUT

-->

<a name="command-line_options_for_the_authentication_and_authorization"></a>
### Command-Line Options for the Authentication and Authorization

`--server.disable-authentication value`

Setting value to true will turn off authentication on the server side so all clients can execute any action without authorization and privilege checks.

The default value is false.

<!--@copydetails triagens::arango::ArangoServer::_disableAuthentication-->

<a name="introduction_to_user_management"></a>
## Introduction to User Management

ArangoDB provides basic functionality to add, modify and remove database users
programmatically. The following functionality is provided by the `users` module
and can be used from inside arangosh and arangod.

Please note that this functionality is not available from within the web
interface.

`users.save(user, passwd, active, extra, changePassword)`

This will create a new ArangoDB user. The username must be specified in
*user* and must not be empty.

The password must be given as a string, too, but can be left empty if required.

If the *active* attribute is not specified, it defaults to `true`. The
*extra* attribute can be used to save custom data with the user.

If the *changePassword* attribute is not specified, it defaults to `false`.
The *changePassword* attribute can be used to indicate that the user must
change has password before logging in.

This method will fail if either the username or the passwords are not specified
or given in a wrong format, or there already exists a user with the specified
name.

The new user account can only be used after the server is either restarted or
the server authentication cache is [reloaded](#examples_reload).

Note: this function will not work from within the web interface

<a name="examples"></a>
#### EXAMPLES

    arangosh> require("org/arangodb/users").save("my-user", "my-secret-password");

`users.document(user)`

Fetches an existing ArangoDB user from the database.

The username must be specified in *user*.

This method will fail if the user cannot be found in the database.

Note: this function will not work from within the web interface

`users.replace(user, passwd, active, extra, changePassword)`

This will look up an existing ArangoDB user and replace its user data.

The username must be specified in *user*, and a user with the specified name
must already exist in the database.

The password must be given as a string, too, but can be left empty if required.

If the *active* attribute is not specified, it defaults to `true`.  The
*extra* attribute can be used to save custom data with the user.

If the *changePassword* attribute is not specified, it defaults to `false`.
The *changePassword* attribute can be used to indicate that the user must
change has password before logging in.

This method will fail if either the username or the passwords are not specified
or given in a wrong format, or if the specified user cannot be found in the
database.

Note: this function will not work from within the web interface

<a name="examples_update"></a>
#### EXAMPLES Update

    arangosh> require("org/arangodb/users").replace("my-user", "my-changed-password");

`users.update(user, passwd, active, extra, changePassword)`

This will update an existing ArangoDB user with a new password and other data.

The username must be specified in *user* and the user must already exist in
the database.

The password must be given as a string, too, but can be left empty if required.

If the *active* attribute is not specified, the current value saved for the
user will not be changed. The same is true for the *extra* and the
*changePassword* attribute.

This method will fail if either the username or the passwords are not specified
or given in a wrong format, or if the specified user cannot be found in the
database.

Note: this function will not work from within the web interface

<a name="examples_remove"></a>
#### EXAMPLES Remove

    arangosh> require("org/arangodb/users").update("my-user", "my-secret-password");

`users.remove(user)`

Removes an existing ArangoDB user from the database.

The username must be specified in @FA{user} and the specified user must exist in
the database.

This method will fail if the user cannot be found in the database.

Note: this function will not work from within the web interface

<a name="examples_reload"></a>
#### EXAMPLES Reload

    arangosh> require("org/arangodb/users").remove("my-user");

`users.reload()`

Reloads the user authentication data on the server

All user authentication data is loaded by the server once on startup only and is
cached after that. When users get added or deleted, a cache flush is required,
and this can be performed by called this method.

Note: this function will not work from within the web interface

`users.isvalid(user, password)`

Checks whether the given combination of username and password is valid.  The
function will return a boolean value if the combination of username and password
is valid.

Each call to this function is penalized by the server sleeping a random
amount of time.

Note: this function will not work from within the web interface

`users.all()`

Fetches all existing ArangoDB users from the database.