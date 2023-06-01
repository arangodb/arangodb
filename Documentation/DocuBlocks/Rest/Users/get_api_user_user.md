@startDocuBlock get_api_user_user

@RESTHEADER{GET /_api/user/{user}, Fetch a user's properties, getUser}

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
Returned if you have *No access* database access level to the `_system`
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
