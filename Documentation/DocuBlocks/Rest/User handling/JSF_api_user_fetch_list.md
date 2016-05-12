
@startDocuBlock JSF_api_user_fetch_list
@brief fetch the properties of a user.

@RESTHEADER{GET /_api/user/, List available Users}

@RESTDESCRIPTION

Fetches data about all users.

The call will return a JSON object with at least the following attributes on success:

- *user*: The name of the user as a string.
- *active*: An optional flag that specifies whether the user is active.
- *extra*: An optional JSON object with arbitrary extra data about the user.
- *changePassword*: An optional flag that specifies whether the user must
  change the password or not.

@RESTRETURNCODES

@RESTRETURNCODE{200}
The users that were found

@endDocuBlock

