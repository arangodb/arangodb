HTTP Interface for User Management{#HttpUser}
=============================================

@NAVIGATE_HttpUser
@EMBEDTOC{HttpUserTOC}

User Management{#HttpUserIntro}
===============================

This is an introduction to ArangoDB's Http interface for managing users.

The interface provides a simple means to add, update, and remove users.  All
users managed through this interface will be stored in the system collection
`_users`.

This specialised interface intentionally does not provide all functionality that
is available in the regular document REST API.

Operations on users may become more restricted than regular document operations,
and extra privileges and security security checks may be introduced in the
future for this interface.

Please note that user operations are not included in ArangoDB's replication.

@COMMENT{######################################################################}
@anchor HttpUserSave
@RESTHEADER{POST /_api/user,creates user}

@RESTBODYPARAM{body,json,required}

@RESTDESCRIPTION

The following data need to be passed in a JSON representation in the body of the
POST request:

- `user`: The name of the user as a string. This is mandatory.

- `passwd`: The user password as a string. If no password is specified, the
  empty string will be used.

- `active`: an optional flag that specifies whether the user is active.  If not
  specified, this will default to `true`.

- `extra`: an optional JSON object with arbitrary extra data about the user.

- `changePassword`: an optional flag that specifies whethers the user must
  change the password or not. If not specified, this will default to `false`.

  If set to `true`, the only operations allowed are `PUT /_api/user` or
  `PATCH /_api/user`. All other operations will result in a `HTTP 403`.

If the user can be added by the server, the server will respond with `HTTP 201`.

In case of success, the returned JSON object has the following properties:

- `error`: boolean flag to indicate that an error occurred (`false` in this
  case)

- `code`: the HTTP status code

If the JSON representation is malformed or mandatory data is missing from the
request, the server will respond with `HTTP 400`.

The body of the response will contain a JSON object with additional error
details. The object has the following attributes:

- `error`: boolean flag to indicate that an error occurred (`true` in this case)

- `code`: the HTTP status code

- `errorNum`: the server error number

- `errorMessage`: a descriptive error message

@RESTRETURNCODES

@RESTRETURNCODE{201}
returned if the user can be added by the server.

@RESTRETURNCODE{400}
If the JSON representation is malformed or mandatory data is missing from the
request.

@RESTDONE

@COMMENT{######################################################################}
@CLEARPAGE
@anchor HttpUserReplace
@RESTHEADER{PUT /_api/user/{user},replaces user}

@RESTURLPARAMETERS

@RESTURLPARAM{user,string,required}
The name of the user.

@RESTBODYPARAM{body,json,required}

@RESTDESCRIPTION

Replaces the data of an existing user. The name of an existing user must be
specified in `user`.

The following data can to be passed in a JSON representation in the body of the
POST request:

- `passwd`: The user password as a string. Specifying a password is mandatory,
  but the empty string is allowed for passwords.

- `active`: an optional flag that specifies whether the user is active.  If not
  specified, this will default to `true`.

- `extra`: an optional JSON object with arbitrary extra data about the user.

- `changePassword`: an optional flag that specifies whether the user must change
  the password or not.  If not specified, this will default to `false`.

If the user can be replaced by the server, the server will respond with `HTTP
200`.

In case of success, the returned JSON object has the following properties:

- `error`: boolean flag to indicate that an error occurred (`false` in this
  case)

- `code`: the HTTP status code

If the JSON representation is malformed or mandatory data is missing from the
request, the server will respond with `HTTP 400`. If the specified user does not
exist, the server will respond with `HTTP 404`.

The body of the response will contain a JSON object with additional error
details. The object has the following attributes:

- `error`: boolean flag to indicate that an error occurred (`true` in this case)

- `code`: the HTTP status code

- `errorNum`: the server error number

- `errorMessage`: a descriptive error message

@RESTRETURNCODES

@RESTRETURNCODE{200}
Is returned if the user data can be replaced by the server.

@RESTRETURNCODE{400}
The JSON representation is malformed or mandatory data is missing from the
request.

@RESTRETURNCODE{404}
The specified user does not exist.

@RESTDONE

@COMMENT{######################################################################}
@CLEARPAGE
@anchor HttpUserUpdate
@RESTHEADER{PATCH /_api/user/{user},updates user}

@RESTURLPARAMETERS

@RESTURLPARAM{user,string,required}
The name of the user.

@RESTBODYPARAM{body,json,required}

@RESTDESCRIPTION

Partially updates the data of an existing user. The name of an existing user
must be specified in `user`.

The following data can be passed in a JSON representation in the body of the
POST request:

- `passwd`: The user password as a string. Specifying a password is optional. If
  not specified, the previously existing value will not be modified.

- `active`: an optional flag that specifies whether the user is active.  If not
  specified, the previously existing value will not be modified.

- `extra`: an optional JSON object with arbitrary extra data about the user. If
  not specified, the previously existing value will not be modified.

- `changePassword`: an optional flag that specifies whether the user must change
  the password or not. If not specified, the previously existing value will not
  be modified.

If the user can be updated by the server, the server will respond with `HTTP
200`.

In case of success, the returned JSON object has the following properties:

- `error`: boolean flag to indicate that an error occurred (`false` in this
  case)

- `code`: the HTTP status code

If the JSON representation is malformed or mandatory data is missing from the
request, the server will respond with `HTTP 400`. If the specified user does not
exist, the server will respond with `HTTP 404`.

The body of the response will contain a JSON object with additional error
details. The object has the following attributes:

- `error`: boolean flag to indicate that an error occurred (`true` in this case)

- `code`: the HTTP status code

- `errorNum`: the server error number

- `errorMessage`: a descriptive error message

@RESTRETURNCODES

@RESTRETURNCODE{200}
Is returned if the user data can be replaced by the server.

@RESTRETURNCODE{400}
The JSON representation is malformed or mandatory data is missing from the
request.

@RESTRETURNCODE{404}
The specified user does not exist.

@RESTDONE

@COMMENT{######################################################################}
@CLEARPAGE
@anchor HttpUserRemove
@RESTHEADER{DELETE /_api/user/{user},removes a user}

@RESTURLPARAMETERS

@RESTURLPARAM{user,string,required}
The name of the user.

@RESTDESCRIPTION

Removes an existing user, identified by `user`.

If the user can be removed, the server will respond with `HTTP 202`.

In case of success, the returned JSON object has the following properties:

- `error`: boolean flag to indicate that an error occurred (`false` in this
  case)

- `code`: the HTTP status code

If the specified user does not exist, the server will respond with `HTTP 404`.

The body of the response will contain a JSON object with additional error
details. The object has the following attributes:

- `error`: boolean flag to indicate that an error occurred (`true` in this case)

- `code`: the HTTP status code

- `errorNum`: the server error number

- `errorMessage`: a descriptive error message

@RESTRETURNCODES

@RESTRETURNCODE{202}
Is returned if the user was removed by the server.

@RESTRETURNCODE{404}
The specified user does not exist.

@RESTDONE

@COMMENT{######################################################################}
@CLEARPAGE
@anchor HttpUserDocument
@RESTHEADER{GET /_api/user/{user},fetches a user}

@RESTURLPARAMETERS

@RESTURLPARAM{user,string,required}
The name of the user.

@RESTDESCRIPTION

Fetches data about the specified user.

The call will return a JSON document with at least the following attributes on
success:

- `user`: The name of the user as a string.

- `active`: an optional flag that specifies whether the user is active.

- `extra`: an optional JSON object with arbitrary extra data about the user.

- `changePassword`: an optional flag that specifies whether the user must change
  the password or not.

@RESTRETURNCODES

@RESTRETURNCODE{200}
The user was found.

@RESTRETURNCODE{404}
The user with `user` does not exist.

@RESTDONE

@COMMENT{######################################################################}
@BNAVIGATE_HttpUser
