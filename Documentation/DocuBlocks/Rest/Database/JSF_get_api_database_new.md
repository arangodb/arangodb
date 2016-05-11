
@startDocuBlock JSF_get_api_database_new
@brief creates a new database

@RESTHEADER{POST /_api/database, Create database}

@RESTBODYPARAM{name,string,required,string}
Has to contain a valid database name.

@RESTBODYPARAM{username,string,optional,string}
The user name as a string.
If *users* is not specified or does not contain any users, a default user
*root* will be created with an empty string password. This ensures that the
new database will be accessible after it is created.

@RESTBODYPARAM{passwd,string,optional,string}
The user password as a string. If not specified, it will default to an empty string.

@RESTBODYPARAM{active,boolean,optional,}
A Flag indicating whether the user account should be activated or not.
The default value is *true*.

@RESTBODYPARAM{extra,object,optional,}
A JSON object with extra user information. The data contained in *extra*
 will be stored for the user but not be interpreted further by ArangoDB.

@RESTBODYPARAM{users,array,optional,JSF_get_api_database_new_USERS}
Has to be a list of user objects to initially create for the new database.
Each user object can contain the following attributes:

@RESTSTRUCT{username,JSF_get_api_database_new_USERS,string,required,string}
Loginname of the user to be created

@RESTSTRUCT{passwd,JSF_get_api_database_new_USERS,string,required,string}
Password for the user

@RESTSTRUCT{active,JSF_get_api_database_new_USERS,boolean,required,}
if *False* the user won't be able to log into the database.

@RESTDESCRIPTION
Creates a new database

The response is a JSON object with the attribute *result* set to *true*.

**Note**: creating a new database is only possible from within the *_system* database.

@RESTRETURNCODES

@RESTRETURNCODE{201}
is returned if the database was created successfully.

@RESTRETURNCODE{400}
is returned if the request parameters are invalid or if a database with the
specified name already exists.

@RESTRETURNCODE{403}
is returned if the request was not executed in the *_system* database.

@RESTRETURNCODE{409}
is returned if a database with the specified name already exists.

@EXAMPLES

Creating a database named *example*.

@EXAMPLE_ARANGOSH_RUN{RestDatabaseCreate}
    var url = "/_api/database";
    var name = "example";
    try {
      db._dropDatabase(name);
    }
    catch (err) {
    }

    var data = {
      name: name
    };
    var response = logCurlRequest('POST', url, data);

    db._dropDatabase(name);
    assert(response.code === 201);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Creating a database named *mydb* with two users.

@EXAMPLE_ARANGOSH_RUN{RestDatabaseCreateUsers}
    var url = "/_api/database";
    var name = "mydb";
    try {
      db._dropDatabase(name);
    }
    catch (err) {
    }

    var data = {
      name: name,
      users: [
        {
          username : "admin",
          passwd : "secret",
          active: true
        },
        {
          username : "tester",
          passwd : "test001",
          active: false
        }
      ]
    };
    var response = logCurlRequest('POST', url, data);

    db._dropDatabase(name);
    assert(response.code === 201);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock

