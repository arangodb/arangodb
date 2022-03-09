
@startDocuBlock get_api_database_new
@brief creates a new database

@RESTHEADER{POST /_api/database, Create database, createDatabase}

@RESTBODYPARAM{name,string,required,string}
Has to contain a valid database name. The name must conform to the selected
naming convention for databases. If the name contains Unicode characters, the
name must be [NFC-normalized](https://en.wikipedia.org/wiki/Unicode_equivalence#Normal_forms).
Non-normalized names will be rejected by arangod.

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
An array of user objects. The users will be granted *Administrate* permissions
for the new database. Users that do not exist yet will be created.
If *users* is not specified or does not contain any users, the default user
*root* will be used to ensure that the new database will be accessible after it
is created. The *root* user is created with an empty password should it not
exist. Each user object can contain the following attributes:

@RESTSTRUCT{username,get_api_database_new_USERS,string,required,}
Login name of an existing user or one to be created.

@RESTSTRUCT{passwd,get_api_database_new_USERS,string,optional,password}
The user password as a string. If not specified, it will default to an empty
string. The attribute is ignored for users that already exist.

@RESTSTRUCT{active,get_api_database_new_USERS,boolean,optional,}
A flag indicating whether the user account should be activated or not.
The default value is *true*. If set to *false*, then the user won't be able to
log into the database. The default is *true*. The attribute is ignored for users
that already exist.

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
