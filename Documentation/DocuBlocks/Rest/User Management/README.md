@startDocuBlock UserHandling_create
@brief Create a new user.

@RESTHEADER{POST /_api/user, Create User}

@RESTBODYPARAM{user,string,required,string}
The name of the user as a string. This is mandatory.

@RESTBODYPARAM{passwd,string,required,string}
The user password as a string. If no password is specified, the empty string
will be used. If you pass the special value *ARANGODB_DEFAULT_ROOT_PASSWORD*,
then the password will be set the value stored in the environment variable
`ARANGODB_DEFAULT_ROOT_PASSWORD`. This can be used to pass an instance
variable into ArangoDB. For example, the instance identifier from Amazon.

@RESTBODYPARAM{active,boolean,optional,}
An optional flag that specifies whether the user is active.  If not
specified, this will default to true

@RESTBODYPARAM{extra,object,optional,}
An optional JSON object with arbitrary extra data about the user.

@RESTDESCRIPTION
Create a new user. You need server access level *Administrate* in order to
execute this REST call.

@RESTRETURNCODES

@RESTRETURNCODE{201}
Returned if the user can be added by the server

@RESTRETURNCODE{400}
If the JSON representation is malformed or mandatory data is missing
from the request.

@RESTRETURNCODE{401}
Returned if you have *No access* database access level to the *_system*
database.

@RESTRETURNCODE{403}
Returned if you have *No access* server access level.

@RESTRETURNCODE{409}
Returned if a user with the same name already exists.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestCreateUser}
    ~try { require("@arangodb/users").remove("admin@example"); } catch (err) {}
    var url = "/_api/user";
    var data = { user: "admin@example", passwd: "secure" };
    var response = logCurlRequest('POST', url, data);

    assert(response.code === 201);

    logJsonResponse(response);
    ~require("@arangodb/users").remove("admin@example");
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock


@startDocuBlock UserHandling_grantDatabase
@brief Set the database access level.

@RESTHEADER{PUT /_api/user/{user}/database/{dbname}, Set the database access level}

@RESTBODYPARAM{grant,string,required,string}
Use "rw" to set the database access level to *Administrate*.<br>
Use "ro" to set the database access level to *Access*.<br>
Use "none" to set the database access level to *No access*.

@RESTURLPARAMETERS

@RESTURLPARAM{user,string,required}
The name of the user.

@RESTURLPARAM{dbname,string,required}
The name of the database.

@RESTDESCRIPTION
Sets the database access levels for the database *dbname* of user *user*. You
need the *Administrate* server access level in order to execute this REST
call.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Returned if the access level was changed successfully.

@RESTRETURNCODE{400}
If the JSON representation is malformed or mandatory data is missing
from the request.

@RESTRETURNCODE{401}
Returned if you have *No access* database access level to the *_system*
database.

@RESTRETURNCODE{403}
Returned if you have *No access* server access level.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestGrantDatabase}
    var users = require("@arangodb/users");
    var theUser = "admin@myapp";
    users.save(theUser, "secret")

    var url = "/_api/user/" + theUser + "/database/_system";
    var data = { grant: "rw" };
    var response = logCurlRequest('PUT', url, data);

    assert(response.code === 200);

    logJsonResponse(response);
    users.remove(theUser);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock


@startDocuBlock UserHandling_revokeDatabase
@brief Clear the database access level, revert back to the default access level

@RESTHEADER{DELETE /_api/user/{user}/database/{dbname}, Clear the database access level}

@RESTURLPARAMETERS

@RESTURLPARAM{user,string,required}
The name of the user.

@RESTURLPARAM{dbname,string,required}
The name of the database.

@RESTDESCRIPTION
Clears the database access level for the database *dbname* of user *user*. As
consequence the default database access level is used. If there is no defined
default database access level, it defaults to *No access*. You need permission
to the *_system* database in order to execute this REST call.

@RESTRETURNCODES

@RESTRETURNCODE{202}
Returned if the access permissions were changed successfully.

@RESTRETURNCODE{400}
If the JSON representation is malformed or mandatory data is missing
from the request.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestRevokeDatabase}
var users = require("@arangodb/users");
var theUser = "admin@myapp";
try { users.remove(theUser); } catch (err) {}
users.save(theUser, "secret")

var url = "/_api/user/" + theUser + "/database/_system";
var response = logCurlRequest('DELETE', url);

assert(response.code === 202);

logJsonResponse(response);
users.remove(theUser);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock


@startDocuBlock UserHandling_grantCollection
@brief Set the collection access level.

@RESTHEADER{PUT /_api/user/{user}/database/{dbname}/{collection}, Set the collection access level}

@RESTBODYPARAM{grant,string,required,string}
Use "rw" to set the collection level access to *Read/Write*.

Use "ro" to set the collection level access to  *Read Only*.

Use "none" to set the collection level access to *No access*.

@RESTURLPARAMETERS

@RESTURLPARAM{user,string,required}
The name of the user.

@RESTURLPARAM{dbname,string,required}
The name of the database.

@RESTURLPARAM{collection,string,required}
The name of the collection.

@RESTDESCRIPTION
Sets the collection access level for the *collection* in the database *dbname*
for user *user*. You need the *Administrate* server access level in order to
execute this REST call.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Returned if the access permissions were changed successfully.

@RESTRETURNCODE{400}
If the JSON representation is malformed or mandatory data is missing
from the request.

@RESTRETURNCODE{401}
Returned if you have *No access* database access level to the *_system*
database.

@RESTRETURNCODE{403}
Returned if you have *No access* server access level.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestGrantCollection}
    var users = require("@arangodb/users");
    var theUser = "admin@myapp";
~   try { users.remove(theUser); } catch (err) {}
~   try { db_drop("reports"); } catch (err) {}
~   db._create("reports");
    users.save(theUser, "secret")

    var url = "/_api/user/" + theUser + "/database/_system/reports";
    var data = { grant: "rw" };
    var response = logCurlRequest('PUT', url, data);

    assert(response.code === 200);
~   db._drop("reports");

    logJsonResponse(response);
    users.remove(theUser);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock


@startDocuBlock UserHandling_revokeCollection
@brief Clear the collection access level, revert back to the default access level

@RESTHEADER{DELETE /_api/user/{user}/database/{dbname}/{collection}, Clear the collection access level}

@RESTURLPARAMETERS

@RESTURLPARAM{user,string,required}
The name of the user.

@RESTURLPARAM{dbname,string,required}
The name of the database.

@RESTURLPARAM{collection,string,required}
The name of the collection.

@RESTDESCRIPTION
Clears the collection access level for the collection *collection* in the
database *dbname* of user *user*.  As consequence the default collection
access level is used. If there is no defined default collection access level,
it defaults to *No access*.  You need permissions to the *_system* database in
order to execute this REST call.

@RESTRETURNCODES

@RESTRETURNCODE{202}
Returned if the access permissions were changed successfully.

@RESTRETURNCODE{400}
If there was an error

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestRevokeCollection}
  var users = require("@arangodb/users");
  var theUser = "admin@myapp";
  try { users.remove(theUser); } catch (err) {}
~ try { db_drop("reports"); } catch (err) {}
~ db._create("reports");
  users.save(theUser, "secret")
  users.grantCollection(theUser, "_system", "reports", "rw");

  var url = "/_api/user/" + theUser + "/database/_system/reports";
  var response = logCurlRequest('DELETE', url);

  assert(response.code === 202);
~ db._drop("reports");

  logJsonResponse(response);
  users.remove(theUser);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock


@startDocuBlock UserHandling_fetchDatabaseList
@brief List the accessible databases for a user

@RESTHEADER{GET /_api/user/{user}/database/, List the accessible databases for a user}

@RESTURLPARAMETERS

@RESTURLPARAM{user,string,required}
The name of the user for which you want to query the databases.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{full,boolean,optional}
Return the full set of access levels for all databases and all collections.

@RESTDESCRIPTION
Fetch the list of databases available to the specified *user*. You need
*Administrate* for the server access level in order to execute this REST call.

The call will return a JSON object with the per-database access
privileges for the specified user. The *result* object will contain
the databases names as object keys, and the associated privileges
for the database as values.

In case you specified *full*, the result will contain the permissions
for the databases as well as the permissions for the collections.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Returned if the list of available databases can be returned.

@RESTRETURNCODE{400}
If the access privileges are not right etc.

@RESTRETURNCODE{401}
Returned if you have *No access* database access level to the *_system*
database.

@RESTRETURNCODE{403}
Returned if you have *No access* server access level.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestFetchUserDatabaseList}
    var users = require("@arangodb/users");
    var theUser="anotherAdmin@secapp";
    users.save(theUser, "secret");
    users.grantDatabase(theUser, "_system", "rw");

    var url = "/_api/user/" + theUser + "/database/";
    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
    users.remove(theUser);
@END_EXAMPLE_ARANGOSH_RUN

With the full response format:

@EXAMPLE_ARANGOSH_RUN{RestFetchUserDatabaseListFull}
var users = require("@arangodb/users");
var theUser="anotherAdmin@secapp";
users.save(theUser, "secret");
users.grantDatabase(theUser, "_system", "rw");

var url = "/_api/user/" + theUser + "/database?full=true";
var response = logCurlRequest('GET', url);

assert(response.code === 200);

logJsonResponse(response);
users.remove(theUser);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock


@startDocuBlock UserHandling_fetchDatabasePermission
@brief Get specific database access level

@RESTHEADER{GET /_api/user/{user}/database/{dbname}, Get the database access level}

@RESTURLPARAMETERS

@RESTURLPARAM{user,string,required}
The name of the user for which you want to query the databases.

@RESTURLPARAM{dbname,string,required}
The name of the database to query

@RESTDESCRIPTION
Fetch the database access level for a specific database

@RESTRETURNCODES

@RESTRETURNCODE{200}
Returned if the access level can be returned

@RESTRETURNCODE{400}
If the access privileges are not right etc.

@RESTRETURNCODE{401}
Returned if you have *No access* database access level to the *_system*
database.

@RESTRETURNCODE{403}
Returned if you have *No access* server access level.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestFetchUserDatabasePermission}
var users = require("@arangodb/users");
var theUser="anotherAdmin@secapp";
users.save(theUser, "secret");
users.grantDatabase(theUser, "_system", "rw");

var url = "/_api/user/" + theUser + "/database/_system";
var response = logCurlRequest('GET', url);

assert(response.code === 200);

logJsonResponse(response);
users.remove(theUser);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock


@startDocuBlock UserHandling_fetchCollectionPermission
@brief Get the collection access level

@RESTHEADER{GET /_api/user/{user}/database/{dbname}/{collection}, Get the specific collection access level}

@RESTURLPARAMETERS

@RESTURLPARAM{user,string,required}
The name of the user for which you want to query the databases.

@RESTURLPARAM{dbname,string,required}
The name of the database to query

@RESTURLPARAM{collection,string,required}
The name of the collection

@RESTDESCRIPTION
Returns the collection access level for a specific collection

@RESTRETURNCODES

@RESTRETURNCODE{200}
Returned if the access level can be returned

@RESTRETURNCODE{400}
If the access privileges are not right etc.

@RESTRETURNCODE{401}
Returned if you have *No access* database access level to the *_system*
database.

@RESTRETURNCODE{403}
Returned if you have *No access* server access level.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestFetchUserCollectionPermission}
var users = require("@arangodb/users");
var theUser="anotherAdmin@secapp";
users.save(theUser, "secret");
users.grantDatabase(theUser, "_system", "rw");

var url = "/_api/user/" + theUser + "/database/_system/_users";
var response = logCurlRequest('GET', url);

assert(response.code === 200);

logJsonResponse(response);
users.remove(theUser);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock


@startDocuBlock UserHandling_replace
@brief Replace an existing user.

@RESTHEADER{PUT /_api/user/{user}, Replace User}

@RESTURLPARAMETERS

@RESTURLPARAM{user,string,required}
The name of the user

@RESTBODYPARAM{passwd,string,required,string}
The user password as a string. Specifying a password is mandatory, but
the empty string is allowed for passwords

@RESTBODYPARAM{active,boolean,optional,}
An optional flag that specifies whether the user is active.  If not
specified, this will default to true

@RESTBODYPARAM{extra,object,optional,}
An optional JSON object with arbitrary extra data about the user.

@RESTDESCRIPTION
Replaces the data of an existing user. The name of an existing user must be
specified in *user*. You need server access level *Administrate* in order to
execute this REST call. Additionally, a user can change his/her own data.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Is returned if the user data can be replaced by the server.

@RESTRETURNCODE{400}
The JSON representation is malformed or mandatory data is missing from the request

@RESTRETURNCODE{401}
Returned if you have *No access* database access level to the *_system*
database.

@RESTRETURNCODE{403}
Returned if you have *No access* server access level.

@RESTRETURNCODE{404}
The specified user does not exist

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestReplaceUser}
    var users = require("@arangodb/users");
    var theUser = "admin@myapp";
    users.save(theUser, "secret")

    var url = "/_api/user/" + theUser;
    var data = { passwd: "secure" };
    var response = logCurlRequest('PUT', url, data);

    assert(response.code === 200);

    logJsonResponse(response);
    users.remove(theUser);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock


@startDocuBlock UserHandling_modify
@brief Modify attributes of an existing user

@RESTHEADER{PATCH /_api/user/{user}, Modify User}

@RESTURLPARAMETERS

@RESTURLPARAM{user,string,required}
The name of the user

@RESTBODYPARAM{passwd,string,required,string}
The user password as a string. Specifying a password is mandatory, but
the empty string is allowed for passwords

@RESTBODYPARAM{active,boolean,optional,}
An optional flag that specifies whether the user is active.  If not
specified, this will default to true

@RESTBODYPARAM{extra,object,optional,}
An optional JSON object with arbitrary extra data about the user.

@RESTDESCRIPTION
Partially updates the data of an existing user. The name of an existing user
must be specified in *user*. You need server access level *Administrate* in
order to execute this REST call. Additionally, a user can change his/her own
data.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Is returned if the user data can be replaced by the server.

@RESTRETURNCODE{400}
The JSON representation is malformed or mandatory data is missing from the request.

@RESTRETURNCODE{401}
Returned if you have *No access* database access level to the *_system*
database.

@RESTRETURNCODE{403}
Returned if you have *No access* server access level.

@RESTRETURNCODE{404}
The specified user does not exist

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestUpdateUser}
    var users = require("@arangodb/users");
    var theUser = "admin@myapp";
    users.save(theUser, "secret")

    var url = "/_api/user/" + theUser;
    var data = { passwd: "secure" };
    var response = logCurlRequest('PATCH', url, data);

    assert(response.code === 200);

    logJsonResponse(response);
    users.remove(theUser);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock

@endDocuBlock


@startDocuBlock UserHandling_delete
@brief delete a user permanently.

@RESTHEADER{DELETE /_api/user/{user}, Remove User}

@RESTURLPARAMETERS

@RESTURLPARAM{user,string,required}
The name of the user

@RESTDESCRIPTION
Removes an existing user, identified by *user*.  You need *Administrate* for
the server access level in order to execute this REST call.

@RESTRETURNCODES

@RESTRETURNCODE{202}
Is returned if the user was removed by the server

@RESTRETURNCODE{401}
Returned if you have *No access* database access level to the *_system*
database.

@RESTRETURNCODE{403}
Returned if you have *No access* server access level.

@RESTRETURNCODE{404}
The specified user does not exist

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestDeleteUser}
    var users = require("@arangodb/users");
    var theUser = "userToDelete@myapp";
    users.save(theUser, "secret")

    var url = "/_api/user/" + theUser;
    var response = logCurlRequest('DELETE', url, {});

    assert(response.code === 202);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock


@startDocuBlock UserHandling_fetch
@brief fetch the properties of a user.

@RESTHEADER{GET /_api/user/{user}, Fetch User}

@RESTURLPARAMETERS

@RESTURLPARAM{user,string,required}
The name of the user

@RESTDESCRIPTION
Fetches data about the specified user. You can fetch information about
yourself or you need the *Administrate* server access level in order to
execute this REST call.

@RESTRETURNCODES

@RESTRETURNCODE{200}
The user was found.

@RESTRETURNCODE{401}
Returned if you have *No access* database access level to the *_system*
database.

@RESTRETURNCODE{403}
Returned if you have *No access* server access level.

@RESTRETURNCODE{404}
The user with the specified name does not exist.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestFetchUser}
    var users = require("@arangodb/users");
    var theUser = "admin@myapp";
    users.save(theUser, "secret")

    var url = "/_api/user/" + theUser;
    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
    users.remove(theUser);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock


@startDocuBlock UserHandling_fetchProperties
@brief fetch the properties of a user.

@RESTHEADER{GET /_api/user/, List available Users}

@RESTDESCRIPTION
Fetches data about all users.  You need the *Administrate* server access level
in order to execute this REST call.  Otherwise, you will only get information
about yourself.

The call will return a JSON object with at least the following
attributes on success:

- *user*: The name of the user as a string.
- *active*: An optional flag that specifies whether the user is active.
- *extra*: An optional JSON object with arbitrary extra data about the user.

@RESTRETURNCODES

@RESTRETURNCODE{200}
The users that were found.

@RESTRETURNCODE{401}
Returned if you have *No access* database access level to the *_system*
database.

@RESTRETURNCODE{403}
Returned if you have *No access* server access level.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestFetchAllUser}
    var url = "/_api/user";
    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
