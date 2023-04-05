@startDocuBlock delete_api_user_user_database_database
@brief Clear the database access level, revert back to the default access level

@RESTHEADER{DELETE /_api/user/{user}/database/{dbname}, Clear the database access level, deleteUserDatabasePermissions}

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
