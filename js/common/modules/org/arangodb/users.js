/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief User management
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

var internal = require("internal"); // OK: encodePassword, reloadAuth

var encodePassword = internal.encodePassword;
var reloadAuth = internal.reloadAuth;
var arangodb = require("org/arangodb");
var db = arangodb.db;
var ArangoError = require("org/arangodb/arango-error").ArangoError;

// -----------------------------------------------------------------------------
// --SECTION--                                       module "org/arangodb/users"
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief validate a username
////////////////////////////////////////////////////////////////////////////////

var validateName = function (username) {
  if (typeof username !== 'string' || username === '') {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_USER_INVALID_NAME.code;
    err.errorMessage = arangodb.errors.ERROR_USER_INVALID_NAME.message;

    throw err;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief validate password
////////////////////////////////////////////////////////////////////////////////

var validatePassword = function (passwd) {
  if (typeof passwd !== 'string') {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_USER_INVALID_PASSWORD.code;
    err.errorMessage = arangodb.errors.ERROR_USER_INVALID_PASSWORD.message;

    throw err;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return the users collection
////////////////////////////////////////////////////////////////////////////////

var getStorage = function () {
  var users = db._collection("_users");

  if (users === null) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code;
    err.errorMessage = "collection _users not found";

    throw err;
  }

  return users;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_saveUser
/// @brief create a new user
///
/// @FUN{users.save(@FA{username}, @FA{passwd})}
///
/// This will create a new ArangoDB user.  The username must be a string and
/// contain only the letters a-z (lower or upper case), the digits 0-9, the dash
/// or the underscore symbol.
///
/// The password must be given as a string, too, but can be left empty if 
/// required.
/// 
/// This method will fail if either the username or the passwords are not
/// specified or given in a wrong format, or there already exists a user with 
/// the specified name.
///
/// The new user account can only be used after the server is either restarted
/// or the server authentication cache is @ref UserManagementReload "reloaded".
///
/// Note: this function will not work from within the web interface
///
/// @EXAMPLES
///
/// @code
/// arangosh> require("users").save("my-user", "my-secret-password");
/// arangosh> require("users").reload();
/// @endcode
////////////////////////////////////////////////////////////////////////////////
  
exports.save = function (username, passwd) {
  if (passwd === null || passwd === undefined) {
    passwd = "";
  }

  // validate input
  validateName(username);
  validatePassword(passwd);
    
  var users = getStorage();
  var user = users.firstExample({ user: username });

  if (user === null) {
    var hash = encodePassword(passwd);
    return users.save({ user: username, password: hash, active: true });
  }
    
  var err = new ArangoError();
  err.errorNum = arangodb.errors.ERROR_USER_DUPLICATE.code;
  err.errorMessage = arangodb.errors.ERROR_USER_DUPLICATE.message;

  throw err;
};

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_replaceUser
/// @brief update an existing user
///
/// @FUN{users.replace(@FA{username}, @FA{passwd})}
///
/// This will update an existing ArangoDB user with a new password.
///
/// The username must be a string and contain only the letters a-z (lower or 
/// upper case), the digits 0-9, the dash or the underscore symbol. The user
/// must already exist in the database.
///
/// The password must be given as a string, too, but can be left empty if 
/// required.
///
/// This method will fail if either the username or the passwords are not
/// specified or given in a wrong format, or if the specified user cannot be
/// found in the database.
///
/// The update is effective only after the server is either restarted
/// or the server authentication cache is @ref UserManagementReload "reloaded".
///
/// Note: this function will not work from within the web interface
///
/// @EXAMPLES
///
/// @code
/// arangosh> require("users").replace("my-user", "my-changed-password");
/// arangosh> require("users").reload();
/// @endcode
////////////////////////////////////////////////////////////////////////////////
  
exports.replace =
exports.update = function (username, passwd) {
  if (passwd === null || passwd === undefined) {
    passwd = "";
  }

  // validate input
  validateName(username);
  validatePassword(passwd);

  var users = getStorage();
  var user = users.firstExample({ user: username });

  if (user === null) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_USER_NOT_FOUND.code;
    err.errorMessage = arangodb.errors.ERROR_USER_NOT_FOUND.message;

    throw err;
  }

  var hash = encodePassword(passwd);
  var data = user.shallowCopy;
  data.password = hash;
  return users.replace(user, data);
};

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_removeUser
/// @brief delete an existing user
///
/// @FUN{users.remove(@FA{username}, @FA{passwd})}
///
/// Removes an existing ArangoDB user from the database.
///
/// The username must be a string and contain only the letters a-z (lower or 
/// upper case), the digits 0-9, the dash or the underscore symbol. The user
/// must already exist in the database.
///
/// This method will fail if the user cannot be found in the database.
///
/// The deletion is effective only after the server is either restarted
/// or the server authentication cache is @ref UserManagementReload "reloaded".
///
/// Note: this function will not work from within the web interface
///
/// @EXAMPLES
///
/// @code
/// arangosh> require("users").remove("my-user");
/// arangosh> require("users").reload();
/// @endcode
////////////////////////////////////////////////////////////////////////////////
  
exports.remove = function (username) {
  // validate input
  validateName(username);

  var users = getStorage();
  var user = users.firstExample({ user: username });

  if (user === null) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_USER_NOT_FOUND.code;
    err.errorMessage = arangodb.errors.ERROR_USER_NOT_FOUND.message;

    throw err;
  }

  return users.remove(user._id);
};

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_reloadUsers
/// @brief reloads the user authentication data
///
/// @FUN{users.reload()}
///
/// Reloads the user authentication data on the server
///
/// All user authentication data is loaded by the server once on startup only
/// and is cached after that. When users get added or deleted, a cache flush is
/// required, and this can be performed by called this method.
///
/// Note: this function will not work from within the web interface
////////////////////////////////////////////////////////////////////////////////
  
exports.reload = function () {
  return reloadAuth();
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\|/\\*jslint"
// End:

