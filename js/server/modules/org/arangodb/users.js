/*jslint indent: 2, nomen: true, maxlen: 120, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports, ArangoAgency */

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
/// @brief encode password using SHA256
////////////////////////////////////////////////////////////////////////////////

var encodePassword = function (password) {
  var salt;
  var encoded;

  var random = crypto.rand();
  if (random === undefined) {
    random = "time:" + internal.time();
  }
  else {
    random = "random:" + random;
  }

  salt = crypto.sha256(random);
  salt = salt.substr(0,8);

  encoded = "$1$" + salt + "$" + crypto.sha256(salt + password);

  return encoded;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief validates a username
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
/// @brief validates password
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

exports.save = function (user, passwd, active, extra, changePassword) {
  if (passwd === null || passwd === undefined) {
    passwd = "";
  }

  // validate input
  validateName(user);
  validatePassword(passwd);

  if (active === undefined || active === null) {
    active = true; // this is the default value
  }

  if (active === undefined || active === null) {
    active = true; // this is the default value
  }

  var users = getStorage();
  var previous = users.firstExample({ user: user });

  if (previous === null) {
    var hash = encodePassword(passwd);
    var data = {
      user: user,
      password: hash,
      active: active,
      changePassword: changePassword
    };

    if (extra !== undefined) {
      data.extra = extra;
    }

    var doc = users.save(data);

    // not exports.reload() as this is an abstract method...
    require("org/arangodb/users").reload();
    return users.document(doc._id);
  }

  var err = new ArangoError();
  err.errorNum = arangodb.errors.ERROR_USER_DUPLICATE.code;
  err.errorMessage = arangodb.errors.ERROR_USER_DUPLICATE.message;

  throw err;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces an existing user
////////////////////////////////////////////////////////////////////////////////

exports.replace = function (user, passwd, active, extra, changePassword) {
  if (passwd === null || passwd === undefined) {
    passwd = "";
  }

  // validate input
  validateName(user);
  validatePassword(passwd);

  if (active === undefined || active === null) {
    active = true; // this is the default
  }

  if (changePassword === undefined || changePassword === null) {
    changePassword = false; // this is the default
  }

  var users = getStorage();
  var previous = users.firstExample({ user: user });

  if (previous === null) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_USER_NOT_FOUND.code;
    err.errorMessage = arangodb.errors.ERROR_USER_NOT_FOUND.message;

    throw err;
  }

  var hash = encodePassword(passwd);
  var data = {
    user: user,
    password: hash,
    active: active,
    changePassword: changePassword
  };

  if (extra !== undefined) {
    data.extra = extra;
  }

  users.replace(previous, data);

  // not exports.reload() as this is an abstract method...
  require("org/arangodb/users").reload();

  return users.document(previous._id);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief updates an existing user
////////////////////////////////////////////////////////////////////////////////

exports.update = function (user, passwd, active, extra, changePassword) {

  // validate input
  validateName(user);

  if (passwd !== undefined) {
    validatePassword(passwd);
  }

  var users = getStorage();
  var previous = users.firstExample({ user: user });

  if (previous === null) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_USER_NOT_FOUND.code;
    err.errorMessage = arangodb.errors.ERROR_USER_NOT_FOUND.message;

    throw err;
  }

  var data = previous._shallowCopy;

  if (passwd !== undefined) {
    var hash = encodePassword(passwd);
    data.password = hash;
  }

  if (active !== undefined && active !== null) {
    data.active = active;
  }

  if (extra !== undefined) {
    data.extra = extra;
  }

  if (changePassword !== undefined && changePassword !== null) {
    data.changePassword = changePassword;
  }

  users.update(previous, data);

  // not exports.reload() as this is an abstract method...
  require("org/arangodb/users").reload();

  return users.document(previous._id);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes an existing user
////////////////////////////////////////////////////////////////////////////////

exports.remove = function (user) {
  // validate input
  validateName(user);

  var users = getStorage();
  var previous = users.firstExample({ user: user });

  if (previous === null) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_USER_NOT_FOUND.code;
    err.errorMessage = arangodb.errors.ERROR_USER_NOT_FOUND.message;

    throw err;
  }

  users.remove(previous);

  // not exports.reload() as this is an abstract method...
  require("org/arangodb/users").reload();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief gets an existing user
////////////////////////////////////////////////////////////////////////////////

exports.document = function (user) {

  // validate name
  validateName(user);

  var users = getStorage();
  var doc = users.firstExample({ user: user });

  if (doc === null) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_USER_NOT_FOUND.code;
    err.errorMessage = arangodb.errors.ERROR_USER_NOT_FOUND.message;

    throw err;
  }

  return {
    user: doc.user,
    active: doc.active,
    extra: doc.extra || {},
    changePassword: doc.changePassword
  };
};

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether a combination of username / password is valid.
////////////////////////////////////////////////////////////////////////////////

exports.isValid = function (user, password) {
  var users = getStorage();
  var previous = users.firstExample({ user: user });

  if (previous === null || ! previous.active) {
    return false;
  }

  var salted = previous.password.substr(3, 8) + password;
  var hex = crypto.sha256(salted);

  // penalize the call
  internal.sleep(Math.random());

  return (previous.password.substr(12) === hex);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief gets all existing users
////////////////////////////////////////////////////////////////////////////////

exports.all = function () {
  var cursor = getStorage().all();
  var result = [ ];

  while (cursor.hasNext()) {
    var doc = cursor.next();
    var user = {
      user: doc.user,
      active: doc.active,
      extra: doc.extra || { },
      changePassword: doc.changePassword
    };

    result.push(user);
  }

  return result;
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

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
// End:

