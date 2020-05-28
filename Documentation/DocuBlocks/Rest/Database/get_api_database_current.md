
@startDocuBlock get_api_database_current
@brief retrieves information about the current database

@RESTHEADER{GET /_api/database/current, Information of the database, getDatabases:current}

@RESTDESCRIPTION
Retrieves the properties of the current database

The response is a JSON object with the following attributes:

- *name*: the name of the current database

- *id*: the id of the current database

- *path*: the filesystem path of the current database

- *isSystem*: whether or not the current database is the *_system* database

- *sharding*: the default sharding method for collections created in this database

- *replicationFactor*: the default replication factor for collections in this database

- *writeConcern*: the default write concern for collections in this database

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the information was retrieved successfully.

@RESTRETURNCODE{400}
is returned if the request is invalid.

@RESTRETURNCODE{404}
is returned if the database could not be found.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestDatabaseGetInfo}
    var url = "/_api/database/current";
    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
