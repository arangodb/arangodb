@startDocuBlock put_api_user_user_database_database

@RESTHEADER{PUT /_api/user/{user}/database/{dbname}, Set a user's database access level, setUserDatabasePermissions}

@RESTBODYPARAM{grant,string,required,string}
- Use "rw" to set the database access level to *Administrate*.
- Use "ro" to set the database access level to *Access*.
- Use "none" to set the database access level to *No access*.

@RESTURLPARAMETERS

@RESTURLPARAM{user,string,required}
The name of the user.

@RESTURLPARAM{dbname,string,required}
The name of the database.

@RESTDESCRIPTION
Sets the database access levels for the database `dbname` of user `user`. You
need the *Administrate* server access level in order to execute this REST
call.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Returned if the access level was changed successfully.

@RESTRETURNCODE{400}
If the JSON representation is malformed or mandatory data is missing
from the request.

@RESTRETURNCODE{401}
Returned if you have *No access* database access level to the `_system`
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
