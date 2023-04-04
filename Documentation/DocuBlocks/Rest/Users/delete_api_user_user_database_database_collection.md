@startDocuBlock delete_api_user_user_database_database_collection
@brief Clear the collection access level, revert back to the default access level

@RESTHEADER{DELETE /_api/user/{user}/database/{dbname}/{collection}, Clear the collection access level, deleteUserCollectionPermissions}

@RESTURLPARAMETERS

@RESTURLPARAM{user,string,required}
The name of the user.

@RESTURLPARAM{dbname,string,required}
The name of the database.

@RESTURLPARAM{collection,string,required}
The name of the collection.

@RESTDESCRIPTION
Clears the collection access level for the collection *collection* in the
database *dbname* of user *user*.  As consequence the default collection
access level is used. If there is no defined default collection access level,
it defaults to *No access*.  You need permissions to the *_system* database in
order to execute this REST call.

@RESTRETURNCODES

@RESTRETURNCODE{202}
Returned if the access permissions were changed successfully.

@RESTRETURNCODE{400}
If there was an error

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestRevokeCollection}
  var users = require("@arangodb/users");
  var theUser = "admin@myapp";
  try { users.remove(theUser); } catch (err) {}
~ try { db_drop("reports"); } catch (err) {}
~ db._create("reports");
  users.save(theUser, "secret")
  users.grantCollection(theUser, "_system", "reports", "rw");

  var url = "/_api/user/" + theUser + "/database/_system/reports";
  var response = logCurlRequest('DELETE', url);

  assert(response.code === 202);
~ db._drop("reports");

  logJsonResponse(response);
  users.remove(theUser);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
