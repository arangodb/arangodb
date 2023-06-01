@startDocuBlock patch_api_user_user

@RESTHEADER{PATCH /_api/user/{user}, Modify a user, updateUserData}

@RESTURLPARAMETERS

@RESTURLPARAM{user,string,required}
The name of the user.

@RESTBODYPARAM{passwd,string,required,string}
The user password as a string.

@RESTBODYPARAM{active,boolean,optional,}
An optional flag that specifies whether the user is active.

@RESTBODYPARAM{extra,object,optional,}
A JSON object with extra user information. It is used by the web interface
to store graph viewer settings and saved queries. Should not be set or
modified by end users, as custom attributes will not be preserved.

@RESTDESCRIPTION
Partially updates the data of an existing user. You need server access level
*Administrate* in order to execute this REST call. Additionally, users can
change their own data.

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
