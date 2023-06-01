@startDocuBlock get_api_user

@RESTHEADER{GET /_api/user/, List available users, listUsers}

@RESTDESCRIPTION
Fetches data about all users. You need the *Administrate* server access level
in order to execute this REST call.  Otherwise, you will only get information
about yourself.

The call will return a JSON object with at least the following
attributes on success:

- `user`: The name of the user as a string.
- `active`: An optional flag that specifies whether the user is active.
- `extra`: A JSON object with extra user information. It is used by the web
  interface to store graph viewer settings and saved queries.

@RESTRETURNCODES

@RESTRETURNCODE{200}
The users that were found.

@RESTRETURNCODE{401}
Returned if you have *No access* database access level to the `_system`
database.

@RESTRETURNCODE{403}
Returned if you have *No access* server access level.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestFetchAllUser}
    var url = "/_api/user";
    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
