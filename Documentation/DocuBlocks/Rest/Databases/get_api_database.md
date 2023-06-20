
@startDocuBlock get_api_database

@RESTHEADER{GET /_api/database, List all databases, listDatabases}

@RESTDESCRIPTION
Retrieves the list of all existing databases

**Note**: retrieving the list of databases is only possible from within the `_system` database.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the list of database was compiled successfully.

@RESTRETURNCODE{400}
is returned if the request is invalid.

@RESTRETURNCODE{403}
is returned if the request was not executed in the `_system` database.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestDatabaseGet}
    var url = "/_api/database";
    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
