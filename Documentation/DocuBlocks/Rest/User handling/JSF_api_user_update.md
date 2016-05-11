
@startDocuBlock JSF_api_user_update
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

