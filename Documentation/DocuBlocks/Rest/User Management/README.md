<!-- ---------------------------------------------------------------------- -->

@startDocuBlock UserHandling_create
@brief Create a new user.

@RESTHEADER{POST /_api/user, Create User}

@RESTBODYPARAM{user,string,required,string}
The name of the user as a string. This is mandatory.

@RESTBODYPARAM{passwd,string,required,string}
The user password as a string. If no password is specified, the empty
string will be used. If you pass the special value
*ARANGODB_DEFAULT_ROOT_PASSWORD*, the password will be set the value
stored in the environment variable
`ARANGODB_DEFAULT_ROOT_PASSWORD`. This can be used to pass an instance
variable into ArangoDB. For example, the instance identifier from
Amazon.

@RESTBODYPARAM{active,boolean,optional,boolean}
An optional flag that specifies whether the user is active.  If not
specified, this will default to true

@RESTBODYPARAM{extra,object,optional,}
An optional JSON object with arbitrary extra data about the user.

@RESTDESCRIPTION

Create a new user. This user will not have access to any database. You
need permission to the *_system* database in order to execute this
REST call.

@RESTRETURNCODES

@RESTRETURNCODE{201}
Returned if the user can be added by the server

@RESTRETURNCODE{400}
If the JSON representation is malformed or mandatory data is missing
from the request.

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

<!-- ---------------------------------------------------------------------- -->

@startDocuBlock UserHandling_grantDatabase
@brief Grant or revoke access to a database.

@RESTHEADER{PUT /_api/user/{user}/database/{dbname}, Grant or revoke database access}

@RESTBODYPARAM{grant,string,required,string}
Use "rw" to grant read and write access rights, or "ro" to
grant read-only access right. To revoke access rights, use "none".

@RESTURLPARAMETERS

@RESTURLPARAM{user,string,required}
The name of the user.

@RESTURLPARAM{dbname,string,required}
The name of the database.

@RESTDESCRIPTION

Grants or revokes access to the database *dbname* for user *user*. You
need permission to the *_system* database in order to execute this
REST call.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Returned if the access permissions were changed successfully.

@RESTRETURNCODE{400}
If the JSON representation is malformed or mandatory data is missing
from the request.

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

<!-- ---------------------------------------------------------------------- -->

@startDocuBlock UserHandling_grantCollection
@brief Grant or revoke access to a collection.

@RESTHEADER{PUT /_api/user/{user}/database/{dbname}/{collection}, Grant or revoke collection access}

@RESTBODYPARAM{grant,string,required,string}
Use "rw" to grant read and write access rights, or "ro" to
grant read-only access right. To revoke access rights, use "none".

@RESTURLPARAMETERS

@RESTURLPARAM{user,string,required}
The name of the user.

@RESTURLPARAM{dbname,string,required}
The name of the database.

@RESTURLPARAM{collection,string,required}
The name of the collection.

@RESTDESCRIPTION

Grants or revokes access to the collection *collection* in the database *dbname* for user *user*. You
need permission to the *_system* database in order to execute this
REST call.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Returned if the access permissions were changed successfully.

@RESTRETURNCODE{400}
If the JSON representation is malformed or mandatory data is missing
from the request.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestGrantCollection}
    var users = require("@arangodb/users");
    var theUser = "admin@myapp";
    users.save(theUser, "secret")

    var url = "/_api/user/" + theUser + "/database/_system/reports";
    var data = { grant: "rw" };
    var response = logCurlRequest('PUT', url, data);

    assert(response.code === 200);

    logJsonResponse(response);
    users.remove(theUser);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock

<!-- ---------------------------------------------------------------------- -->

@startDocuBlock UserHandling_fetchDatabaseList
@brief List available database to the specified user

@RESTHEADER{GET /_api/user/{user}/database/, List the databases available to a User}

@RESTURLPARAMETERS

@RESTURLPARAM{user,string,required}
The name of the user for which you want to query the databases.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{full,boolean,optional}
Return the full set of permissions for all databases and all collections.

@RESTDESCRIPTION

Fetch the list of databases available to the specified *user*. You
need permission to the *_system* database in order to execute this
REST call.

The call will return a JSON object with the per-database access
privileges for the specified user. The *result* object will contain
the databases names as object keys, and the associated privileges
for the database as values.

In case you specified *full*, the result will contain the permissions
for the databases as well as the permissions for the 


@RESTRETURNCODES

@RESTRETURNCODE{200}
Returned if the list of available databases can be returned.

@RESTRETURNCODE{400}
If the access privileges are not right etc.

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

<!-- ---------------------------------------------------------------------- -->

@startDocuBlock UserHandling_fetchDatabasePermission
@brief Get specific database permissions

@RESTHEADER{GET /_api/user/{user}/database/{database}, Get the db permission level}

@RESTURLPARAMETERS

@RESTURLPARAM{user,string,required}
The name of the user for which you want to query the databases.

@RESTURLPARAM{database,string,required}
The name of the database to query

@RESTDESCRIPTION

Fetch the permission level for a specific database


@RESTRETURNCODES

@RESTRETURNCODE{200}
Returned if the acccess level can be returned

@RESTRETURNCODE{400}
If the access privileges are not right etc.

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

<!-- ---------------------------------------------------------------------- -->

@startDocuBlock UserHandling_fetchCollectionPermission
@brief Get specific collection permissions

@RESTHEADER{GET /_api/user/{user}/database/{database}/{collection}, Get the collection permission level}

@RESTURLPARAMETERS

@RESTURLPARAM{user,string,required}
The name of the user for which you want to query the databases.

@RESTURLPARAM{database,string,required}
The name of the database to query

@RESTURLPARAM{collection,string,required}
The name of the collection


@RESTDESCRIPTION

Fetch the permission level for a specific collection

@RESTRETURNCODES

@RESTRETURNCODE{200}
Returned if the acccess level can be returned

@RESTRETURNCODE{400}
If the access privileges are not right etc.

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

<!-- ---------------------------------------------------------------------- -->

@startDocuBlock UserHandling_replace
@brief replace an existing user with a new one.

@RESTHEADER{PUT /_api/user/{user}, Replace User}

@RESTURLPARAMETERS

@RESTURLPARAM{user,string,required}
The name of the user

@RESTBODYPARAM{passwd,string,required,string}
The user password as a string. Specifying a password is mandatory, but
the empty string is allowed for passwords

@RESTBODYPARAM{active,boolean,optional,boolean}
An optional flag that specifies whether the user is active.  If not
specified, this will default to true

@RESTBODYPARAM{extra,object,optional,}
An optional JSON object with arbitrary extra data about the user.

@RESTDESCRIPTION

Replaces the data of an existing user. The name of an existing user
must be specified in *user*. When authentication is turned on in the
server, only users that have read and write permissions for the *_system*
database can change other users' data. Additionally, a user can change 
his/her own data.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Is returned if the user data can be replaced by the server

@RESTRETURNCODE{400}
The JSON representation is malformed or mandatory data is missing from the request

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

<!-- ---------------------------------------------------------------------- -->

@startDocuBlock UserHandling_modify
@brief modify attributes of an existing user

@RESTHEADER{PATCH /_api/user/{user}, Update User}

@RESTURLPARAMETERS

@RESTURLPARAM{user,string,required}
The name of the user

@RESTBODYPARAM{passwd,string,required,string}
The user password as a string. Specifying a password is mandatory, but
the empty string is allowed for passwords

@RESTBODYPARAM{active,boolean,optional,boolean}
An optional flag that specifies whether the user is active.  If not
specified, this will default to true

@RESTBODYPARAM{extra,object,optional,}
An optional JSON object with arbitrary extra data about the user.

@RESTDESCRIPTION

Partially updates the data of an existing user. The name of an existing
user must be specified in *user*. When authentication is turned on in the
server, only users that have read and write permissions for the *_system*
database can change other users' data. Additionally, a user can change 
his/her own data.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Is returned if the user data can be replaced by the server

@RESTRETURNCODE{400}
The JSON representation is malformed or mandatory data is missing from the request

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

<!-- ---------------------------------------------------------------------- -->

@startDocuBlock UserHandling_delete
@brief delete a user permanently.

@RESTHEADER{DELETE /_api/user/{user}, Remove User}

@RESTURLPARAMETERS

@RESTURLPARAM{user,string,required}
The name of the user

@RESTDESCRIPTION

Removes an existing user, identified by *user*. You need access to the
*_system* database.

@RESTRETURNCODES

@RESTRETURNCODE{202}
Is returned if the user was removed by the server

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

<!-- ---------------------------------------------------------------------- -->

@startDocuBlock UserHandling_fetch
@brief fetch the properties of a user.

@RESTHEADER{GET /_api/user/{user}, Fetch User}

@RESTURLPARAMETERS

@RESTURLPARAM{user,string,required}
The name of the user

@RESTDESCRIPTION

Fetches data about the specified user. You can fetch information about yourself
or you need permission to the *_system* database in order to execute this
REST call.

@RESTRETURNCODES

@RESTRETURNCODE{200}
The user was found.

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

<!-- ---------------------------------------------------------------------- -->

@startDocuBlock UserHandling_fetchProperties
@brief fetch the properties of a user.

@RESTHEADER{GET /_api/user/, List available Users}

@RESTDESCRIPTION

Fetches data about all users. You can only execute this call if you
have access to the *_system* database. Otherwise, you will only
get information about yourself.

The call will return a JSON object with at least the following
attributes on success:

- *user*: The name of the user as a string.
- *active*: An optional flag that specifies whether the user is active.
- *extra*: An optional JSON object with arbitrary extra data about the user.
- *changePassword*: An optional flag that specifies whether the user must
  change the password or not.

@RESTRETURNCODES

@RESTRETURNCODE{200}
The users that were found.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestFetchAllUser}
    var url = "/_api/user";
    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
