@startDocuBlock post_api_user

@RESTHEADER{POST /_api/user, Create a user, createUser}

@RESTBODYPARAM{user,string,required,string}
The name of the user as a string. This is mandatory.

@RESTBODYPARAM{passwd,string,required,string}
The user password as a string. If not specified, it will default to an empty
string.

@RESTBODYPARAM{active,boolean,optional,}
An optional flag that specifies whether the user is active. If not
specified, this will default to `true`.

@RESTBODYPARAM{extra,object,optional,}
A JSON object with extra user information. It is used by the web interface
to store graph viewer settings and saved queries. Should not be set or
modified by end users, as custom attributes will not be preserved.

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
Returned if you have *No access* database access level to the `_system`
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
