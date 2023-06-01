@startDocuBlock get_api_user_user_database

@RESTHEADER{GET /_api/user/{user}/database/, List a user's accessible databases, listUserDatabases},

@RESTURLPARAMETERS

@RESTURLPARAM{user,string,required}
The name of the user for which you want to query the databases.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{full,boolean,optional}
Return the full set of access levels for all databases and all collections.

@RESTDESCRIPTION
Fetch the list of databases available to the specified `user`.

You need *Administrate* permissions for the server access level in order to
execute this REST call.

The call will return a JSON object with the per-database access
privileges for the specified user. The `result` object will contain
the databases names as object keys, and the associated privileges
for the database as values.

In case you specified `full`, the result will contain the permissions
for the databases as well as the permissions for the collections.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Returned if the list of available databases can be returned.

@RESTRETURNCODE{400}
If the access privileges are not right etc.

@RESTRETURNCODE{401}
Returned if you have *No access* database access level to the `_system`
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
