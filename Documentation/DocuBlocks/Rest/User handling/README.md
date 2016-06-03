<!-- ---------------------------------------------------------------------- -->

@startDocuBlock UserHandling_create
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
If the JSON representation is malformed or mandatory data is missing
from the request.

@endDocuBlock

<!-- ---------------------------------------------------------------------- -->

@startDocuBlock UserHandling_replace
@brief replace an existing user with a new one.

@RESTHEADER{PUT /_api/user/{user}, Replace User}

@RESTURLPARAMETERS

@RESTURLPARAM{user,string,required}
The name of the user

@RESTDESCRIPTION

Replaces the data of an existing user. The name of an existing user must be specified in user.

The following data can to be passed in a JSON representation in the body of the POST request:

- *passwd*: The user password as a string. Specifying a password is mandatory,
  but the empty string is allowed for passwords
- *active*: An optional flag that specifies whether the user is active.
  If not specified, this will default to true
- *extra*: An optional JSON object with arbitrary extra data about the user
- *changePassword*: An optional flag that specifies whether the user must change
  the password or not. If not specified, this will default to false

If the user can be replaced by the server, the server will respond with HTTP 200.

In case of success, the returned JSON object has the following properties:

- *error*: Boolean flag to indicate that an error occurred (false in this case)
- *code*: The HTTP status code

In case of error, the body of the response will contain a JSON object with additional
error details. The object has the following attributes:

- *error*: Boolean flag to indicate that an error occurred (true in this case)
- *code*: The HTTP status code
- *errorNum*: The server error number
- *errorMessage*: A descriptive error message

@RESTRETURNCODES

@RESTRETURNCODE{200}
Is returned if the user data can be replaced by the server

@RESTRETURNCODE{400}
The JSON representation is malformed or mandatory data is missing from the request

@RESTRETURNCODE{404}
The specified user does not exist

@endDocuBlock

<!-- ---------------------------------------------------------------------- -->

@startDocuBlock UserHandling_modify
@brief modify attributes of an existing user

@RESTHEADER{PATCH /_api/user/{user}, Update User}

@RESTURLPARAMETERS

@RESTURLPARAM{user,string,required}
The name of the user

@RESTDESCRIPTION

Partially updates the data of an existing user. The name of an existing
user must be specified in *user*.

The following data can be passed in a JSON representation in the body of the
POST request:

- *passwd*: The user password as a string. Specifying a password is optional.
  If not specified, the previously existing value will not be modified.
- *active*: An optional flag that specifies whether the user is active.
  If not specified, the previously existing value will not be modified.
- *extra*: An optional JSON object with arbitrary extra data about the user.
  If not specified, the previously existing value will not be modified.
- *changePassword*: An optional flag that specifies whether the user must change
  the password or not. If not specified, the previously existing value will not be modified.

If the user can be updated by the server, the server will respond with HTTP 200.

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

@RESTRETURNCODE{200}
Is returned if the user data can be replaced by the server

@RESTRETURNCODE{400}
The JSON representation is malformed or mandatory data is missing from the request

@RESTRETURNCODE{404}
The specified user does not exist

@endDocuBlock

<!-- ---------------------------------------------------------------------- -->

@startDocuBlock UserHandling_delete
@brief delete a user permanently.

@RESTHEADER{DELETE /_api/user/{user}, Remove User}

@RESTURLPARAMETERS

@RESTURLPARAM{user,string,required}
The name of the user

@RESTDESCRIPTION

Removes an existing user, identified by *user*.

If the user can be removed, the server will respond with HTTP 202.
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

@RESTRETURNCODE{202}
Is returned if the user was removed by the server

@RESTRETURNCODE{404}
The specified user does not exist

@endDocuBlock

<!-- ---------------------------------------------------------------------- -->

@startDocuBlock UserHandling_fetch
@brief fetch the properties of a user.

@RESTHEADER{GET /_api/user/{user}, Fetch User}

@RESTURLPARAMETERS

@RESTURLPARAM{user,string,required}
The name of the user

@RESTDESCRIPTION

Fetches data about the specified user.

The call will return a JSON object with at least the following attributes on success:

- *user*: The name of the user as a string.
- *active*: An optional flag that specifies whether the user is active.
- *extra*: An optional JSON object with arbitrary extra data about the user.
- *changePassword*: An optional flag that specifies whether the user must
  change the password or not.

@RESTRETURNCODES

@RESTRETURNCODE{200}
The user was found

@RESTRETURNCODE{404}
The user with the specified name does not exist

@endDocuBlock

<!-- ---------------------------------------------------------------------- -->

@startDocuBlock UserHandling_fetchProperties
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
