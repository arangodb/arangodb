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
  if (typeof username !== 'string' || ! username.match(/^[a-zA-Z0-9\-_]+$/)) {
    throw "username must be a string";
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief validate password
////////////////////////////////////////////////////////////////////////////////

var validatePassword = function (passwd) {
  if (typeof passwd !== 'string') {
    throw "password must be a string";
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return the users collection
////////////////////////////////////////////////////////////////////////////////

var getStorage = function () {
  var users = db._collection("_users");

  if (users === null) {
    throw "collection _users does not exist.";
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
/// @brief create a new user
///
/// @FUN{@FA{users}.save(@FA{username}, @FA{passwd})}
///
/// This will create a new ArangoDB user.
/// The username must be a string and contain only the letters a-z (lower or 
/// upper case), the digits 0-9, the dash or the underscore symbol.
///
/// The password must be given as a string, too, but can be left empty if 
/// required.
/// 
/// This method will fail if either the username or the passwords are not
/// specified or given in a wrong format, or there already exists a user with 
/// the specified name.
///
/// The new user account can only be used after the server is either restarted
/// or the server authentication cache is reloaded (see @ref JSF_reloadUsers).
///
/// Note: this function will not work from within the web interface
///
/// @EXAMPLES
///
/// @TINYEXAMPLE{user-save,saving a new user}
////////////////////////////////////////////////////////////////////////////////
  
exports.save = function (username, passwd) {
  // validate input
  validateName(username);
  validatePassword(passwd);
    
  var users = getStorage();
  var user = users.firstExample({ user: username });

  if (user === null) {
    var hash = encodePassword(passwd);
    return users.save({ user: username, password: hash, active: true });
  }

  throw "cannot create user: user already exists.";
};

////////////////////////////////////////////////////////////////////////////////
/// @brief update an existing user
///
/// @FUN{@FA{users}.replace(@FA{username}, @FA{passwd})}
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
/// or the server authentication cache is reloaded (see @ref JSF_reloadUsers).
///
/// Note: this function will not work from within the web interface
///
/// @EXAMPLES
///
/// @TINYEXAMPLE{user-replace,replacing an existing user}
////////////////////////////////////////////////////////////////////////////////
  
exports.replace =
exports.update = function (username, passwd) {
  // validate input
  validateName(username);
  validatePassword(passwd);

  var users = getStorage();
  var user = users.firstExample({ user: username });

  if (user === null) {
    throw "cannot update user: user does not exist.";
  }

  var hash = encodePassword(passwd);
  var data = user.shallowCopy;
  data.password = hash;
  return users.replace(user, data);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief delete an existing user
///
/// @FUN{@FA{users}.remove(@FA{username}, @FA{passwd})}
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
/// or the server authentication cache is reloaded (see @ref JSF_reloadUsers).
///
/// Note: this function will not work from within the web interface
///
/// @EXAMPLES
///
/// @TINYEXAMPLE{user-remove,removing an existing user}
////////////////////////////////////////////////////////////////////////////////
  
exports.remove = function (username) {
  // validate input
  validateName(username);

  var users = getStorage();
  var user = users.firstExample({ user: username });

  if (user === null) {
    throw "cannot delete: user does not exist.";
  }

  return users.remove(user._id);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief reloads the user authentication data
///
/// @FUN{@FA{users}.reload()}
///
/// Reloads the user authentication data on the server
///
/// All user authentication data is loaded by the server once on startup only
/// and is cached after that. When users get added or deleted, a cache flush is
/// required, and this can be performed by called this method.
///
/// Note: this function will not work from within the web interface
/// @anchor JSF_reloadUsers
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

