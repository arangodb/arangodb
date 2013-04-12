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
/// @author Achim Brandt
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
/// @RESTHEADER{GET /_api/user,fetches a user}
///
/// @REST{GET /_api/user/@FA{username}}
///
/// Fetches data about the specified user.
///
/// The call will return a JSON document with at least the following attributes
/// on success:
///
/// - @LIT{username}: The name of the user as a string.
///
/// - @LIT{active}: an optional flag that specifies whether the user is active.
///
/// - @LIT{extra}: an optional JSON object with arbitrary extra data about the
///   user.
////////////////////////////////////////////////////////////////////////////////

function GET_api_user (req, res) {
  if (req.suffix.length != 1) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  var username = decodeURIComponent(req.suffix[0]);
  try {
    var result = users.document(username);
    actions.resultOk(req, res, actions.HTTP_OK, result)
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
/// @REST{POST /_api/user}
///
/// The following data need to be passed in a JSON representation in the body of
/// the POST request:
///
/// - @LIT{username}: The name of the user as a string. This is mandatory.
///
/// - @LIT{passwd}: The user password as a string. If no password is specified,
///   the empty string will be used.
///
/// - @LIT{active}: an optional flag that specifies whether the user is active.
///   If not specified, this will default to @LIT{true}.
///
/// - @LIT{extra}: an optional JSON object with arbitrary extra data about the
///   user.
///
/// If the user can be added by the server, the server will respond with 
/// @LIT{HTTP 201}.
///
/// In case of success, the returned JSON object has the following properties:
///
/// - @LIT{error}: boolean flag to indicate that an error occurred (@LIT{false}
///   in this case)
///
/// - @LIT{code}: the HTTP status code
///
/// If the JSON representation is malformed or mandatory data is missing from the
/// request, the server will respond with @LIT{HTTP 400}.
///
/// The body of the response will contain a JSON object with additional error
/// details. The object has the following attributes:
///
/// - @LIT{error}: boolean flag to indicate that an error occurred (@LIT{true} in this case)
///
/// - @LIT{code}: the HTTP status code
///
/// - @LIT{errorNum}: the server error number
///
/// - @LIT{errorMessage}: a descriptive error message
////////////////////////////////////////////////////////////////////////////////

function POST_api_user (req, res) {
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
/// @RESTHEADER{PUT /_api/user,replaces user}
///
/// @REST{PUT /_api/user/@FA{username}}
///
/// Replaces the data of an existing user. The name of an existing user must
/// be specified in @FA{username}.
///
/// The following data can to be passed in a JSON representation in the body of
/// the POST request:
///
/// - @LIT{passwd}: The user password as a string. Specifying a password is
///   mandatory, but the empty string is allowed for passwords.
///
/// - @LIT{active}: an optional flag that specifies whether the user is active.
///   If not specified, this will default to @LIT{true}.
///
/// - @LIT{extra}: an optional JSON object with arbitrary extra data about the
///   user.
///
/// If the user can be replaced by the server, the server will respond with 
/// @LIT{HTTP 200}. 
///
/// In case of success, the returned JSON object has the following properties:
///
/// - @LIT{error}: boolean flag to indicate that an error occurred (@LIT{false}
///   in this case)
///
/// - @LIT{code}: the HTTP status code
///
/// If the JSON representation is malformed or mandatory data is missing from the
/// request, the server will respond with @LIT{HTTP 400}. If the specified user
/// does not exist, the server will respond with @LIT{HTTP 404}.
///
/// The body of the response will contain a JSON object with additional error
/// details. The object has the following attributes:
///
/// - @LIT{error}: boolean flag to indicate that an error occurred (@LIT{true} in this case)
///
/// - @LIT{code}: the HTTP status code
///
/// - @LIT{errorNum}: the server error number
///
/// - @LIT{errorMessage}: a descriptive error message
////////////////////////////////////////////////////////////////////////////////

function PUT_api_user (req, res) {
  if (req.suffix.length != 1) {
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
/// @RESTHEADER{PATCH /_api/user,updates user}
///
/// @REST{PATCH /_api/user/@FA{username}}
///
/// Partially updates the data of an existing user. The name of an existing user 
/// must be specified in @FA{username}.
///
/// The following data can be passed in a JSON representation in the body of
/// the POST request:
///
/// - @LIT{passwd}: The user password as a string. Specifying a password is
///   optional. If not specified, the previously existing value will not be
///   modified.
///
/// - @LIT{active}: an optional flag that specifies whether the user is active.
///   If not specified, the previously existing value will not be modified.
///
/// - @LIT{extra}: an optional JSON object with arbitrary extra data about the
///   user. If not specified, the previously existing value will not be modified.
///
/// If the user can be updated by the server, the server will respond with 
/// @LIT{HTTP 200}. 
///
/// In case of success, the returned JSON object has the following properties:
///
/// - @LIT{error}: boolean flag to indicate that an error occurred (@LIT{false}
///   in this case)
///
/// - @LIT{code}: the HTTP status code
///
/// If the JSON representation is malformed or mandatory data is missing from the
/// request, the server will respond with @LIT{HTTP 400}. If the specified user
/// does not exist, the server will respond with @LIT{HTTP 404}.
///
/// The body of the response will contain a JSON object with additional error
/// details. The object has the following attributes:
///
/// - @LIT{error}: boolean flag to indicate that an error occurred (@LIT{true} in this case)
///
/// - @LIT{code}: the HTTP status code
///
/// - @LIT{errorNum}: the server error number
///
/// - @LIT{errorMessage}: a descriptive error message
////////////////////////////////////////////////////////////////////////////////

function PATCH_api_user (req, res) {
  if (req.suffix.length != 1) {
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
/// @RESTHEADER{DELETE /_api/user,removes a user}
///
/// @REST{DELETE /_api/user/@FA{username}}
///
/// Removes an existing user, identified by @FA{username}.
///
/// If the user can be removed by the server, the server will respond with 
/// @LIT{HTTP 202}. 
///
/// In case of success, the returned JSON object has the following properties:
///
/// - @LIT{error}: boolean flag to indicate that an error occurred (@LIT{false}
///   in this case)
///
/// - @LIT{code}: the HTTP status code
///
/// If the JSON representation is malformed or mandatory data is missing from the
/// request, the server will respond with @LIT{HTTP 400}. If the specified user
/// does not exist, the server will respond with @LIT{HTTP 404}.
///
/// The body of the response will contain a JSON object with additional error
/// details. The object has the following attributes:
///
/// - @LIT{error}: boolean flag to indicate that an error occurred (@LIT{true} in this case)
///
/// - @LIT{code}: the HTTP status code
///
/// - @LIT{errorNum}: the server error number
///
/// - @LIT{errorMessage}: a descriptive error message
////////////////////////////////////////////////////////////////////////////////

function DELETE_api_user (req, res) {
  if (req.suffix.length != 1) {
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
          GET_api_user(req, res); 
          break;

        case actions.POST: 
          POST_api_user(req, res); 
          break;

        case actions.PUT:  
          PUT_api_user(req, res); 
          break;
        
        case actions.PATCH:  
          PATCH_api_user(req, res); 
          break;

        case actions.DELETE:  
          DELETE_api_user(req, res); 
          break;

        default:
          actions.resultUnsupported(req, res);
      }
    }
    catch (err) {
      actions.resultException(req, res, err);
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
