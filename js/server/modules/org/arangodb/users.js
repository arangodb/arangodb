/*jshint strict: false */
/*global ArangoAgency */

////////////////////////////////////////////////////////////////////////////////
/// @brief User management
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

var internal = require("internal"); // OK: reloadAuth
var arangodb = require("org/arangodb");
var crypto = require("org/arangodb/crypto");

var db = arangodb.db;
var ArangoError = arangodb.ArangoError;

// -----------------------------------------------------------------------------
// --SECTION--                                       module "org/arangodb/users"
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a user document to the legacy format
////////////////////////////////////////////////////////////////////////////////

var convertToLegacyFormat = function (doc) {
  return {
    user: doc.user,
    active: doc.authData.active,
    extra: doc.userData || {},
    changePassword: doc.authData.changePassword
  };
};

////////////////////////////////////////////////////////////////////////////////
/// @brief encode password using SHA256
////////////////////////////////////////////////////////////////////////////////

var hashPassword = function (password) {
  var salt = internal.genRandomAlphaNumbers(16);

  return {
    hash: crypto.sha256(salt + password),
    salt: salt,
    method: "sha256"
  };
};

////////////////////////////////////////////////////////////////////////////////
/// @brief validates a username
////////////////////////////////////////////////////////////////////////////////

var validateName = function (username) {
  if (typeof username !== "string" || username === "") {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_USER_INVALID_NAME.code;
    err.errorMessage = arangodb.errors.ERROR_USER_INVALID_NAME.message;

    throw err;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief validates password
////////////////////////////////////////////////////////////////////////////////

var validatePassword = function (password) {
  if (typeof password !== "string") {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_USER_INVALID_PASSWORD.code;
    err.errorMessage = arangodb.errors.ERROR_USER_INVALID_PASSWORD.message;

    throw err;
  }

  if (password === "ARANGODB_DEFAULT_ROOT_PASSWORD") {
    password = require("process").env['ARANGODB_DEFAULT_ROOT_PASSWORD'] || "";
  }

  return password;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the users collection
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

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new user
////////////////////////////////////////////////////////////////////////////////

exports.save = function (username, password, active, userData, changePassword) {
  if (password === null || password === undefined) {
    password = "";
  }

  // validate input
  validateName(username);
  password = validatePassword(password);

  if (active === undefined || active === null) {
    active = true; // this is the default value
  }

  if (active === undefined || active === null) {
    active = true; // this is the default value
  }

  if (changePassword === undefined || changePassword === null) {
    changePassword = false; // this is the default
  }

  var users = getStorage();
  var user = users.firstExample({user: username});

  if (user !== null) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_USER_DUPLICATE.code;
    err.errorMessage = arangodb.errors.ERROR_USER_DUPLICATE.message;
    throw err;
  }

  var data = {
    user: username,
    userData: userData || {},
    authData: {
      simple: hashPassword(password),
      active: Boolean(active),
      changePassword: Boolean(changePassword)
    }
  };

  var doc = users.save(data);

  // not exports.reload() as this is an abstract method...
  require("org/arangodb/users").reload();

  return convertToLegacyFormat(users.document(doc._id));
};

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces an existing user
////////////////////////////////////////////////////////////////////////////////

exports.replace = function (username, password, active, userData, changePassword) {
  if (password === null || password === undefined) {
    password = "";
  }

  // validate input
  validateName(username);
  password = validatePassword(password);

  if (active === undefined || active === null) {
    active = true; // this is the default
  }

  if (changePassword === undefined || changePassword === null) {
    changePassword = false; // this is the default
  }

  var users = getStorage();
  var user = users.firstExample({user: username});

  if (user === null) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_USER_NOT_FOUND.code;
    err.errorMessage = arangodb.errors.ERROR_USER_NOT_FOUND.message;
    throw err;
  }

  var data = {
    user: username,
    userData: userData || {},
    authData: {
      simple: hashPassword(password),
      active: Boolean(active),
      changePassword: Boolean(changePassword)
    }
  };

  var doc = users.replace(user, data);

  // not exports.reload() as this is an abstract method...
  require("org/arangodb/users").reload();

  return convertToLegacyFormat(users.document(doc._id));
};

////////////////////////////////////////////////////////////////////////////////
/// @brief updates an existing user
////////////////////////////////////////////////////////////////////////////////

exports.update = function (username, password, active, userData, changePassword) {
  // validate input
  validateName(username);

  if (password !== undefined) {
    password = validatePassword(password);
  }

  var users = getStorage();
  var user = users.firstExample({user: username});

  if (user === null) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_USER_NOT_FOUND.code;
    err.errorMessage = arangodb.errors.ERROR_USER_NOT_FOUND.message;
    throw err;
  }

  var data = user._shallowCopy;

  if (password !== undefined) {
    data.authData.simple = hashPassword(password);
  }

  if (active !== undefined && active !== null) {
    data.authData.active = active;
  }

  if (userData !== undefined) {
    data.userData = userData;
  }

  if (changePassword !== undefined && changePassword !== null) {
    data.authData.changePassword = changePassword;
  }

  users.update(user, data);

  // not exports.reload() as this is an abstract method...
  require("org/arangodb/users").reload();

  return convertToLegacyFormat(users.document(user._id));
};

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes an existing user
////////////////////////////////////////////////////////////////////////////////

exports.remove = function (username) {
  // validate input
  validateName(username);

  var users = getStorage();
  var user = users.firstExample({user: username});

  if (user === null) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_USER_NOT_FOUND.code;
    err.errorMessage = arangodb.errors.ERROR_USER_NOT_FOUND.message;
    throw err;
  }

  // not exports.reload() as this is an abstract method...
  require("org/arangodb/users").reload();

  users.remove(user);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief gets an existing user
////////////////////////////////////////////////////////////////////////////////

exports.document = function (username) {
  // validate name
  validateName(username);

  var users = getStorage();
  var user = users.firstExample({user: username});

  if (user === null) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_USER_NOT_FOUND.code;
    err.errorMessage = arangodb.errors.ERROR_USER_NOT_FOUND.message;
    throw err;
  }

  return convertToLegacyFormat(user);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether a combination of username / password is valid.
////////////////////////////////////////////////////////////////////////////////

exports.isValid = function (username, password) {
  // validate name
  validateName(username);

  var users = getStorage();
  var user = users.firstExample({user: username});

  if (user === null || !user.authData.active) {
    return false;
  }

  // penalize the call
  internal.sleep(Math.random());

  var hash = crypto[user.authData.simple.method](user.authData.simple.salt + password);
  return crypto.constantEquals(user.authData.simple.hash, hash);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief gets all existing users
////////////////////////////////////////////////////////////////////////////////

exports.all = function () {
  var users = getStorage();

  return users.all().toArray().map(convertToLegacyFormat);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief reloads the user authentication data
////////////////////////////////////////////////////////////////////////////////

exports.reload = function () {
  internal.reloadAuth();

  if (require("org/arangodb/cluster").isCoordinator()) {
    // Tell the agency about this reload, such that all other coordinators
    // reload as well. This is important because most calls to this
    // function here come from actual changes in the collection _users.
    var UserVersion;
    var done = false;

    while (! done) {
      try {
        UserVersion = ArangoAgency.get("Sync/UserVersion")["Sync/UserVersion"];
        // This is now a string!
      }
      catch (err) {
        break;
      }
      try {
        done = ArangoAgency.cas("Sync/UserVersion",UserVersion,
                                (parseInt(UserVersion,10)+1).toString());
      }
      catch (err2) {
        break;
      }
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief sets a password-change token
////////////////////////////////////////////////////////////////////////////////

exports.setPasswordToken = function (username, token) {
  var users = getStorage();
  var user = users.firstExample({user: username});
  if (user === null) {
    return null;
  }

  if (token === null || token === undefined) {
    token = internal.genRandomAlphaNumbers(50);
  }

  users.update(user, {authData: {passwordToken: token}});

  return token;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the password-change token
////////////////////////////////////////////////////////////////////////////////

exports.userByToken = function (token) {
  var users = getStorage();
  return users.firstExample({"authData.passwordToken": token});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the password-change token
////////////////////////////////////////////////////////////////////////////////

exports.changePassword = function (token, password) {
  var users = getStorage();
  var user = users.firstExample({'authData.passwordToken': token});

  if (user === null) {
    return false;
  }

  password = validatePassword(password);

  var authData = user._shallowCopy.authData;

  delete authData.passwordToken;
  authData.simple = hashPassword(password);
  authData.changePassword = false;

  users.update(user, {authData: authData});

  // not exports.reload() as this is an abstract method...
  require("org/arangodb/users").reload();

  return true;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
// End:

