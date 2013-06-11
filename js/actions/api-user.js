/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require */

////////////////////////////////////////////////////////////////////////////////
/// @brief user management
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var arangodb = require("org/arangodb");
var actions = require("org/arangodb/actions");
var users = require("org/arangodb/users");

var ArangoError = arangodb.ArangoError; 

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief fetch a user
///
/// @RESTHEADER{GET /_api/user/{username},fetches a user}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{username,string,required}
/// The username of the user.
///
/// @RESTDESCRIPTION
///
/// Fetches data about the specified user.
///
/// The call will return a JSON document with at least the following attributes
/// on success:
///
/// - `username`: The name of the user as a string.
///
/// - `active`: an optional flag that specifies whether the user is active.
///
/// - `extra`: an optional JSON object with arbitrary extra data about the
///   user.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// The user was found.
///
/// @RESTRETURNCODE{404}
/// The user with `username` does not exist.
///
////////////////////////////////////////////////////////////////////////////////

function get_api_user (req, res) {
  if (req.suffix.length !== 1) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  var username = decodeURIComponent(req.suffix[0]);
  try {
    var result = users.document(username);
    actions.resultOk(req, res, actions.HTTP_OK, result);
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
/// @brief create a new user
///
/// @RESTHEADER{POST /_api/user,creates user}
///
/// @RESTBODYPARAM{user,json,required}
///
/// @RESTDESCRIPTION
///
/// The following data need to be passed in a JSON representation in the body of
/// the POST request:
///
/// - `username`: The name of the user as a string. This is mandatory.
///
/// - `passwd`: The user password as a string. If no password is specified,
///   the empty string will be used.
///
/// - `active`: an optional flag that specifies whether the user is active.
///   If not specified, this will default to `true`.
///
/// - `extra`: an optional JSON object with arbitrary extra data about the
///   user.
///
/// If the user can be added by the server, the server will respond with 
/// `HTTP 201`.
///
/// In case of success, the returned JSON object has the following properties:
///
/// - `error`: boolean flag to indicate that an error occurred (`false`
///   in this case)
///
/// - `code`: the HTTP status code
///
/// If the JSON representation is malformed or mandatory data is missing from the
/// request, the server will respond with `HTTP 400`.
///
/// The body of the response will contain a JSON object with additional error
/// details. The object has the following attributes:
///
/// - `error`: boolean flag to indicate that an error occurred (`true` in this case)
///
/// - `code`: the HTTP status code
///
/// - `errorNum`: the server error number
///
/// - `errorMessage`: a descriptive error message
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{201}
/// returned if the user can be added by the server.
///
/// @RESTRETURNCODE{400}
/// If the JSON representation is malformed or mandatory data is missing from the
/// request.
///
////////////////////////////////////////////////////////////////////////////////

function post_api_user (req, res) {
  var json = actions.getJsonBody(req, res, actions.HTTP_BAD);

  if (json === undefined) {
    return;
  }

  var result = users.save(json.username, json.passwd, json.active, json.extra);
  users.reload();

  actions.resultOk(req, res, actions.HTTP_CREATED, { });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replace an existing user
///
/// @RESTHEADER{PUT /_api/user/{username},replaces user}
///
/// @RESTBODYPARAM{user,json,required}
///
/// @RESTDESCRIPTION
///
/// Replaces the data of an existing user. The name of an existing user must
/// be specified in `username`.
///
/// The following data can to be passed in a JSON representation in the body of
/// the POST request:
///
/// - `passwd`: The user password as a string. Specifying a password is
///   mandatory, but the empty string is allowed for passwords.
///
/// - `active`: an optional flag that specifies whether the user is active.
///   If not specified, this will default to `true`.
///
/// - `extra`: an optional JSON object with arbitrary extra data about the
///   user.
///
/// If the user can be replaced by the server, the server will respond with 
/// `HTTP 200`. 
///
/// In case of success, the returned JSON object has the following properties:
///
/// - `error`: boolean flag to indicate that an error occurred (`false`
///   in this case)
///
/// - `code`: the HTTP status code
///
/// If the JSON representation is malformed or mandatory data is missing from the
/// request, the server will respond with `HTTP 400`. If the specified user
/// does not exist, the server will respond with `HTTP 404`.
///
/// The body of the response will contain a JSON object with additional error
/// details. The object has the following attributes:
///
/// - `error`: boolean flag to indicate that an error occurred (`true` in this case)
///
/// - `code`: the HTTP status code
///
/// - `errorNum`: the server error number
///
/// - `errorMessage`: a descriptive error message
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// Is returned if the user data can be replaced by the server.
///
/// @RESTRETURNCODE{400}
/// The JSON representation is malformed or mandatory data is missing from the
/// request.
///
/// @RESTRETURNCODE{404}
/// The specified user does not exist.
////////////////////////////////////////////////////////////////////////////////

function put_api_user (req, res) {
  if (req.suffix.length !== 1) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  var username = decodeURIComponent(req.suffix[0]);

  var json = actions.getJsonBody(req, res, actions.HTTP_BAD);
  
  if (json === undefined) {
    return;
  }
  
  try {
    var result = users.replace(username, json.passwd, json.active, json.extra);
    users.reload();
  
    actions.resultOk(req, res, actions.HTTP_OK, { });
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
/// @brief partially update an existing user
///
/// @RESTHEADER{PATCH /_api/user/{username},updates user}
///
/// @RESTBODYPARAM{userdata,json,required}
///
/// @RESTDESCRIPTION
///
/// Partially updates the data of an existing user. The name of an existing user 
/// must be specified in `username`.
///
/// The following data can be passed in a JSON representation in the body of
/// the POST request:
///
/// - `passwd`: The user password as a string. Specifying a password is
///   optional. If not specified, the previously existing value will not be
///   modified.
///
/// - `active`: an optional flag that specifies whether the user is active.
///   If not specified, the previously existing value will not be modified.
///
/// - `extra`: an optional JSON object with arbitrary extra data about the
///   user. If not specified, the previously existing value will not be modified.
///
/// If the user can be updated by the server, the server will respond with 
/// `HTTP 200`. 
///
/// In case of success, the returned JSON object has the following properties:
///
/// - `error`: boolean flag to indicate that an error occurred (`false`
///   in this case)
///
/// - `code`: the HTTP status code
///
/// If the JSON representation is malformed or mandatory data is missing from the
/// request, the server will respond with `HTTP 400`. If the specified user
/// does not exist, the server will respond with `HTTP 404`.
///
/// The body of the response will contain a JSON object with additional error
/// details. The object has the following attributes:
///
/// - `error`: boolean flag to indicate that an error occurred (`true` in this case)
///
/// - `code`: the HTTP status code
///
/// - `errorNum`: the server error number
///
/// - `errorMessage`: a descriptive error message
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// Is returned if the user data can be replaced by the server.
///
/// @RESTRETURNCODE{400}
/// The JSON representation is malformed or mandatory data is missing from the
/// request.
///
/// @RESTRETURNCODE{404}
/// The specified user does not exist.
///
////////////////////////////////////////////////////////////////////////////////

function patch_api_user (req, res) {
  if (req.suffix.length !== 1) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  var username = decodeURIComponent(req.suffix[0]);

  var json = actions.getJsonBody(req, res, actions.HTTP_BAD);
  
  if (json === undefined) {
    return;
  }
  
  try {
    var result = users.update(username, json.passwd, json.active, json.extra);
    users.reload();
    actions.resultOk(req, res, actions.HTTP_OK, { });
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
/// @brief remove an existing user
///
/// @RESTHEADER{DELETE /_api/user/{username},removes a user}
///
/// @RESTDESCRIPTION
///
/// Removes an existing user, identified by `username`.
///
/// If the user can be removed by the server, the server will respond with 
/// `HTTP 202`. 
///
/// In case of success, the returned JSON object has the following properties:
///
/// - `error`: boolean flag to indicate that an error occurred (`false`
///   in this case)
///
/// - `code`: the HTTP status code
///
/// If the specified user does not exist, the server will respond with 
/// `HTTP 404`.
///
/// The body of the response will contain a JSON object with additional error
/// details. The object has the following attributes:
///
/// - `error`: boolean flag to indicate that an error occurred (`true` in this case)
///
/// - `code`: the HTTP status code
///
/// - `errorNum`: the server error number
///
/// - `errorMessage`: a descriptive error message
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{202}
/// Is returned if the user was removed by the server.
///
/// @RESTRETURNCODE{404}
/// The specified user does not exist.
///
////////////////////////////////////////////////////////////////////////////////

function delete_api_user (req, res) {
  if (req.suffix.length !== 1) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  var username = decodeURIComponent(req.suffix[0]);
  try {
    var result = users.remove(username);
    users.reload();
    actions.resultOk(req, res, actions.HTTP_ACCEPTED, { });
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
// --SECTION--                                                       initialiser
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief user actions gateway 
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_api/user",
  context : "api",

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

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
