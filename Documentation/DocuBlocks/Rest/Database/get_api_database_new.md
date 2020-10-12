
@startDocuBlock get_api_database_new
@brief creates a new database

@RESTHEADER{POST /_api/database, Create database, createDatabase}

@RESTBODYPARAM{name,string,required,string}
Has to contain a valid database name.

@RESTBODYPARAM{options,object,optional,get_api_database_new_OPTIONS}
Optional object which can contain the following attributes:

@RESTSTRUCT{sharding,get_api_database_new_OPTIONS,string,optional,}
The sharding method to use for new collections in this database. Valid values
are: "", "flexible", or "single". The first two are equivalent. _(cluster only)_

@RESTSTRUCT{replicationFactor,get_api_database_new_OPTIONS,integer,optional,}
Default replication factor for new collections created in this database.
Special values include "satellite", which will replicate the collection to
every DB-Server (Enterprise Edition only), and 1, which disables replication.
_(cluster only)_

@RESTSTRUCT{writeConcern,get_api_database_new_OPTIONS,number,optional,}
Default write concern for new collections created in this database.
It determines how many copies of each shard are required to be
in sync on the different DB-Servers. If there are less then these many copies
in the cluster a shard will refuse to write. Writes to shards with enough
up-to-date copies will succeed at the same time however. The value of
*writeConcern* can not be larger than *replicationFactor*. _(cluster only)_

@RESTBODYPARAM{users,array,optional,get_api_database_new_USERS}
Has to be an array of user objects to initially create for the new database.
User information will not be changed for users that already exist.
If *users* is not specified or does not contain any users, a default user
*root* will be created with an empty string password. This ensures that the
new database will be accessible after it is created.
Each user object can contain the following attributes:

@RESTSTRUCT{username,get_api_database_new_USERS,string,required,}
Login name of the user to be created.

@RESTSTRUCT{passwd,get_api_database_new_USERS,string,optional,password}
The user password as a string. If not specified, it will default to an empty string.

@RESTSTRUCT{active,get_api_database_new_USERS,boolean,optional,}
A flag indicating whether the user account should be activated or not.
The default value is *true*. If set to *false*, the user won't be able to
log into the database. The default is *true*.

@RESTSTRUCT{extra,get_api_database_new_USERS,object,optional,}
A JSON object with extra user information. It is used by the web interface
to store graph viewer settings and saved queries. Should not be set or
modified by end users, as custom attributes will not be preserved.

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
