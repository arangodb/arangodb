
@startDocuBlock JSF_api_user_create
@brief Create a new user.

@RESTHEADER{POST /_api/user, Create User}

@RESTDESCRIPTION

The following data need to be passed in a JSON representation in the body
of the POST request:

- *user*: The name of the user as a string. This is mandatory.
- *passwd*: The user password as a string. If no password is specified, the
  empty string will be used. If you pass the special value
  *ARANGODB_DEFAULT_ROOT_PASSWORD*, the password will be set the value
  stored in the environment variable `ARANGODB_DEFAULT_ROOT_PASSWORD`. This
  can be used to pass an instance variable into ArangoDB. For example, the
  instance identifier from Amazon.
- *active*: An optional flag that specifies whether the user is active.
  If not specified, this will default to true
- *extra*: An optional JSON object with arbitrary extra data about the user
- *changePassword*: An optional flag that specifies whethers the user must
  change the password or not. If not specified, this will default to false.
  If set to true, the only operations allowed are PUT /_api/user or PATCH /_api/user.
  All other operations executed by the user will result in an HTTP 403.

If the user can be added by the server, the server will respond with HTTP 201.
In case of success, the returned JSON object has the following properties:

- *error*: Boolean flag to indicate that an error occurred (false in this case)
- *code*: The HTTP status code

In case of error, the body of the response will contain a JSON object with additional error details.
The object has the following attributes:

- *error*: Boolean flag to indicate that an error occurred (true in this case)
- *code*: The HTTP status code
- *errorNum*: The server error number
- *errorMessage*: A descriptive error message

@RESTRETURNCODES

@RESTRETURNCODE{201}
Returned if the user can be added by the server

@RESTRETURNCODE{400}
If the JSON representation is malformed or mandatory data is missing from the request.

@endDocuBlock

