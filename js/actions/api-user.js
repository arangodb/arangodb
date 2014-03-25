/*jslint indent: 2, nomen: true, maxlen: 120, sloppy: true, vars: true, white: true, plusplus: true */
/*global require */

////////////////////////////////////////////////////////////////////////////////
/// @brief user management
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
  users.reload();

  actions.resultOk(req, res, actions.HTTP_CREATED, doc);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces an existing user
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

function delete_api_user (req, res) {
  if (req.suffix.length !== 1) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  var user = decodeURIComponent(req.suffix[0]);
  try {
    var doc = users.remove(user);
    users.reload();

    actions.resultOk(req, res, actions.HTTP_ACCEPTED, doc);
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

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
// End:
