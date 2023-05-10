@startDocuBlock put_api_user_user_database_database_collection
@brief Set the collection access level.

@RESTHEADER{PUT /_api/user/{user}/database/{dbname}/{collection}, Set the collection access level, setUserCollectionPermissions}

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
