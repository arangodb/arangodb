
@startDocuBlock get_api_database_new
@brief creates a new database

@RESTHEADER{POST /_api/database, Create database, createDatabase}

@RESTBODYPARAM{name,string,required,string}
Has to contain a valid database name.

@RESTBODYPARAM{options,object,optional,get_api_database_new_USERS}
Optional Object which can contain the following attributes:

@RESTSTRUCT{sharing,get_api_database_new_USERS,string,optional,string}
A flag indicating the sharding method to use. Valid values are: 
"", "flexible", or "single". The first two are equivalent

@RESTSTRUC{sharing,get_api_database_new_USERS,string,optional,number}
Default replication-factor for collections created in this new database.
Special values include 0, which is equivalent to "satellite" and will replicate
the collection to every DB-server, and 1, which disables replication.

@RESTBODYPARAM{users,array,optional,get_api_database_new_USERS}
Has to be an array of user objects to initially create for the new database.
User information will not be changed for users that already exist.
If *users* is not specified or does not contain any users, a default user
*root* will be created with an empty string password. This ensures that the
new database will be accessible after it is created.
Each user object can contain the following attributes:

@RESTSTRUCT{username,get_api_database_new_USERS,string,required,string}
Login name of the user to be created

@RESTSTRUCT{passwd,get_api_database_new_USERS,string,required,string}
The user password as a string. If not specified, it will default to an empty string.

@RESTSTRUCT{active,get_api_database_new_USERS,boolean,required,}
A flag indicating whether the user account should be activated or not.
The default value is *true*. If set to *false*, the user won't be able to
log into the database.

@RESTSTRUCT{extra,get_api_database_new_USERS,object,optional,}
A JSON object with extra user information. The data contained in *extra*
will be stored for the user but not be interpreted further by ArangoDB.

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
      name: name,
      options: {
        sharding: "flexible",
        replicationFactor: 3
      }
    };
    var response = logCurlRequest('POST', url, data);

    db._dropDatabase(name);
    assert(response.code === 201);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Creating a database named *mydb* with two users, flexible sharding and
default replication factor of 3 for collections that will be part of 
the newly created database.

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
          username: "admin",
          passwd: "secret",
          active: true
        },
        {
          username: "tester",
          passwd: "test001",
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

