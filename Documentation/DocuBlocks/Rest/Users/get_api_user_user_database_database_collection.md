@startDocuBlock get_api_user_user_database_database_collection

@RESTHEADER{GET /_api/user/{user}/database/{dbname}/{collection}, Get a user's collection access level, getUserCollectionPermissions}

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
Returned if you have *No access* database access level to the `_system`
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
