@startDocuBlock delete_api_user_user

@RESTHEADER{DELETE /_api/user/{user}, Remove a user, deleteUser}

@RESTURLPARAMETERS

@RESTURLPARAM{user,string,required}
The name of the user

@RESTDESCRIPTION
Removes an existing user, identified by `user`. 

You need *Administrate* permissions for the server access level in order to
execute this REST call.

@RESTRETURNCODES

@RESTRETURNCODE{202}
Is returned if the user was removed by the server

@RESTRETURNCODE{401}
Returned if you have *No access* database access level to the `_system`
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
