/*jshint strict: false */
/*global require */

////////////////////////////////////////////////////////////////////////////////
/// @brief user management
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var arangodb = require("org/arangodb");
var actions = require("org/arangodb/actions");
var users = require("org/arangodb/users");

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief fetches a user
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @COMMENT{######################################################################}
/// @anchor HttpUserDocument
/// @startDocuBlock JSF_api_user_fetch
///
/// @RESTHEADER{GET /_api/user/{user}, Fetch User}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{user,string,optional}
/// The name of the user
///
/// @RESTDESCRIPTION
///
/// Fetches data about the specified user.
///
/// The call will return a JSON object with at least the following attributes on success:
///
/// - *user*: The name of the user as a string.
/// - *active*: An optional flag that specifies whether the user is active.
/// - *extra*: An optional JSON object with arbitrary extra data about the user.
/// - *changePassword*: An optional flag that specifies whether the user must
///   change the password or not.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// The user was found
///
/// @RESTRETURNCODE{404}
/// The user with the specified name does not exist
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function get_api_user (req, res) {
  if (req.suffix.length === 0) {
    actions.resultOk(req, res, actions.HTTP_OK, { result: users.all() });
    return;
  }

  if (req.suffix.length !== 1) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  var user = decodeURIComponent(req.suffix[0]);

  try {
    actions.resultOk(req, res, actions.HTTP_OK, users.document(user));
  }
  catch (err) {
    if (err.errorNum === arangodb.errors.ERROR_USER_NOT_FOUND.code) {
      actions.resultNotFound(req, res, arangodb.errors.ERROR_USER_NOT_FOUND.code);
    }
    else {
      throw err;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new user
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @COMMENT{######################################################################}
/// @anchor HttpUserSave
/// @startDocuBlock JSF_api_user_create
///
/// @RESTHEADER{POST /_api/user, Create User}
///
/// @RESTDESCRIPTION
///
/// The following data need to be passed in a JSON representation in the body
/// of the POST request:
///
/// - *user*: The name of the user as a string. This is mandatory
/// - *passwd*: The user password as a string. If no password is specified,
///   the empty string will be used
/// - *active*: An optional flag that specifies whether the user is active.
///   If not specified, this will default to true
/// - *extra*: An optional JSON object with arbitrary extra data about the user
/// - *changePassword*: An optional flag that specifies whethers the user must
///   change the password or not. If not specified, this will default to false.
///   If set to true, the only operations allowed are PUT /_api/user or PATCH /_api/user.
///   All other operations executed by the user will result in an HTTP 403.
///
/// If the user can be added by the server, the server will respond with HTTP 201.
/// In case of success, the returned JSON object has the following properties:
///
/// - *error*: Boolean flag to indicate that an error occurred (false in this case)
/// - *code*: The HTTP status code
///
/// In case of error, the body of the response will contain a JSON object with additional error details.
/// The object has the following attributes:
///
/// - *error*: Boolean flag to indicate that an error occurred (true in this case)
/// - *code*: The HTTP status code
/// - *errorNum*: The server error number
/// - *errorMessage*: A descriptive error message
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{201}
/// Returned if the user can be added by the server
///
/// @RESTRETURNCODE{400}
/// If the JSON representation is malformed or mandatory data is missing from the request.
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function post_api_user (req, res) {
  var json = actions.getJsonBody(req, res, actions.HTTP_BAD);

  if (json === undefined) {
    return;
  }

  var user;

  if (req.suffix.length === 1) {
    // validate if a combination or username / password is valid
    user = decodeURIComponent(req.suffix[0]);
    var result = users.isValid(user, json.passwd);

    if (result) {
      actions.resultOk(req, res, actions.HTTP_OK, { result: true });
    }
    else {
      actions.resultNotFound(req, res, arangodb.errors.ERROR_USER_NOT_FOUND.code);
    }
    return;
  }

  if (req.suffix.length !== 0) {
    // unexpected URL
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  user = json.user;

  if (user === undefined && json.hasOwnProperty("username")) {
    // deprecated usage
    user = json.username;
  }

  var doc = users.save(user, json.passwd, json.active, json.extra, json.changePassword);

  if (json.passwordToken) {
    users.setPasswordToken(user, json.passwordToken);
  }
  users.reload();

  actions.resultOk(req, res, actions.HTTP_CREATED, doc);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces an existing user
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_api_user_replace
///
/// @RESTHEADER{PUT /_api/user/{user}, Replace User}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{user,string,optional}
/// The name of the user
///
/// @RESTDESCRIPTION
///
/// Replaces the data of an existing user. The name of an existing user must be specified in user.
///
/// The following data can to be passed in a JSON representation in the body of the POST request:
///
/// - *passwd*: The user password as a string. Specifying a password is mandatory,
///   but the empty string is allowed for passwords
/// - *active*: An optional flag that specifies whether the user is active.
///   If not specified, this will default to true
/// - *extra*: An optional JSON object with arbitrary extra data about the user
/// - *changePassword*: An optional flag that specifies whether the user must change
///   the password or not. If not specified, this will default to false
///
/// If the user can be replaced by the server, the server will respond with HTTP 200.
///
/// In case of success, the returned JSON object has the following properties:
///
/// - *error*: Boolean flag to indicate that an error occurred (false in this case)
/// - *code*: The HTTP status code
///
/// In case of error, the body of the response will contain a JSON object with additional
/// error details. The object has the following attributes:
///
/// - *error*: Boolean flag to indicate that an error occurred (true in this case)
/// - *code*: The HTTP status code
/// - *errorNum*: The server error number
/// - *errorMessage*: A descriptive error message
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// Is returned if the user data can be replaced by the server
///
/// @RESTRETURNCODE{400}
/// The JSON representation is malformed or mandatory data is missing from the request
///
/// @RESTRETURNCODE{404}
/// The specified user does not exist
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function put_api_user (req, res) {
  if (req.suffix.length !== 1) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  var user = decodeURIComponent(req.suffix[0]);

  var json = actions.getJsonBody(req, res, actions.HTTP_BAD);

  if (json === undefined) {
    return;
  }

  try {
    var doc = users.replace(user, json.passwd, json.active, json.extra, json.changePassword);
    users.reload();

    actions.resultOk(req, res, actions.HTTP_OK, doc);
  }
  catch (err) {
    if (err.errorNum === arangodb.errors.ERROR_USER_NOT_FOUND.code) {
      actions.resultNotFound(req, res, arangodb.errors.ERROR_USER_NOT_FOUND.code);
    }
    else {
      throw err;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief partially updates an existing user
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @COMMENT{######################################################################}
/// @anchor HttpUserUpdate
/// @startDocuBlock JSF_api_user_update
///
/// @RESTHEADER{PATCH /_api/user/{user}, Update User}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{user,string,optional}
/// The name of the user
///
/// @RESTDESCRIPTION
///
/// Partially updates the data of an existing user. The name of an existing
/// user must be specified in *user*.
///
/// The following data can be passed in a JSON representation in the body of the
/// POST request:
///
/// - *passwd*: The user password as a string. Specifying a password is optional.
///   If not specified, the previously existing value will not be modified.
/// - *active*: An optional flag that specifies whether the user is active.
///   If not specified, the previously existing value will not be modified.
/// - *extra*: An optional JSON object with arbitrary extra data about the user.
///   If not specified, the previously existing value will not be modified.
/// - *changePassword*: An optional flag that specifies whether the user must change
///   the password or not. If not specified, the previously existing value will not be modified.
///
/// If the user can be updated by the server, the server will respond with HTTP 200.
///
/// In case of success, the returned JSON object has the following properties:
///
/// - *error*: Boolean flag to indicate that an error occurred (false in this case)
/// - *code*: The HTTP status code
///
/// In case of error, the body of the response will contain a JSON object with additional error details.
/// The object has the following attributes:
///
/// - *error*: Boolean flag to indicate that an error occurred (true in this case)
/// - *code*: The HTTP status code
/// - *errorNum*: The server error number
/// - *errorMessage*: A descriptive error message
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// Is returned if the user data can be replaced by the server
///
/// @RESTRETURNCODE{400}
/// The JSON representation is malformed or mandatory data is missing from the request
///
/// @RESTRETURNCODE{404}
/// The specified user does not exist
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function patch_api_user (req, res) {
  if (req.suffix.length !== 1) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  var user = decodeURIComponent(req.suffix[0]);
  var json = actions.getJsonBody(req, res, actions.HTTP_BAD);

  if (json === undefined) {
    return;
  }

  try {
    var doc = users.update(user, json.passwd, json.active, json.extra, json.changePassword);
    users.reload();

    actions.resultOk(req, res, actions.HTTP_OK, doc);
  }
  catch (err) {
    if (err.errorNum === arangodb.errors.ERROR_USER_NOT_FOUND.code) {
      actions.resultNotFound(req, res, arangodb.errors.ERROR_USER_NOT_FOUND.code);
    }
    else {
      throw err;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an existing user
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_api_user_delete
///
/// @RESTHEADER{DELETE /_api/user/{user}, Remove User}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{user,string,optional}
/// The name of the user
///
/// @RESTDESCRIPTION
///
/// Removes an existing user, identified by *user*.
///
/// If the user can be removed, the server will respond with HTTP 202.
/// In case of success, the returned JSON object has the following properties:
///
/// - *error*: Boolean flag to indicate that an error occurred (false in this case)
/// - *code*: The HTTP status code
///
/// In case of error, the body of the response will contain a JSON object with additional error details.
/// The object has the following attributes:
///
/// - *error*: Boolean flag to indicate that an error occurred (true in this case)
/// - *code*: The HTTP status code
/// - *errorNum*: The server error number
/// - *errorMessage*: A descriptive error message
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{202}
/// Is returned if the user was removed by the server
///
/// @RESTRETURNCODE{404}
/// The specified user does not exist
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function delete_api_user (req, res) {
  if (req.suffix.length !== 1) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  var user = decodeURIComponent(req.suffix[0]);
  try {
    users.remove(user);
    users.reload();

    actions.resultOk(req, res, actions.HTTP_ACCEPTED, {});
  }
  catch (err) {
    if (err.errorNum === arangodb.errors.ERROR_USER_NOT_FOUND.code) {
      actions.resultNotFound(req, res, arangodb.errors.ERROR_USER_NOT_FOUND.code);
    }
    else {
      throw err;
    }
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                           actions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief user actions gateway
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_api/user",

  callback : function (req, res) {
    try {
      switch (req.requestType) {
        case actions.GET:
          get_api_user(req, res);
          break;

        case actions.POST:
          post_api_user(req, res);
          break;

        case actions.PUT:
          put_api_user(req, res);
          break;

        case actions.PATCH:
          patch_api_user(req, res);
          break;

        case actions.DELETE:
          delete_api_user(req, res);
          break;

        default:
          actions.resultUnsupported(req, res);
      }
    }
    catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
