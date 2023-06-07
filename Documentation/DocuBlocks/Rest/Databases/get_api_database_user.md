
@startDocuBlock get_api_database_user

@RESTHEADER{GET /_api/database/user, List the accessible databases, listUserAccessibleDatabases}

@RESTDESCRIPTION
Retrieves the list of all databases the current user can access without
specifying a different username or password.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the list of database was compiled successfully.

@RESTRETURNCODE{400}
is returned if the request is invalid.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestDatabaseGetUser}
    var url = "/_api/database/user";
    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
