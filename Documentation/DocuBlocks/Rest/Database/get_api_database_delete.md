
@startDocuBlock get_api_database_delete
@brief drop an existing database

@RESTHEADER{DELETE /_api/database/{database-name}, Drop database, deleteDatabase}

@RESTURLPARAMETERS

@RESTURLPARAM{database-name,string,required}
The name of the database

@RESTDESCRIPTION
Drops the database along with all data stored in it.

**Note**: dropping a database is only possible from within the *_system* database.
The *_system* database itself cannot be dropped.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the database was dropped successfully.

@RESTRETURNCODE{400}
is returned if the request is malformed.

@RESTRETURNCODE{403}
is returned if the request was not executed in the *_system* database.

@RESTRETURNCODE{404}
is returned if the database could not be found.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestDatabaseDrop}
    var url = "/_api/database";
    var name = "example";

    db._createDatabase(name);
    var response = logCurlRequest('DELETE', url + '/' + name);

    assert(response.code === 200);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
