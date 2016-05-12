
@startDocuBlock JSF_api_user_delete
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

