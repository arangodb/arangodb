/* jshint strict: false */
/* global ArangoAgency */

'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief User management
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// / @author Copyright 2012-2014, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const internal = require('internal'); // OK: reloadAuth
const arangodb = require('@arangodb');
const shallowCopy = require('@arangodb/util').shallowCopy;
const crypto = require('@arangodb/crypto');

const db = arangodb.db;
const ArangoError = arangodb.ArangoError;

// converts a user document to the legacy format
const convertToLegacyFormat = function (doc) {
  return {
    user: doc.user,
    active: doc.authData.active,
    extra: doc.userData || {},
    changePassword: doc.authData.changePassword
  };
};

// encode password using SHA256
const hashPassword = function (password) {
  const salt = internal.genRandomAlphaNumbers(16);

  return {
    hash: crypto.sha256(salt + password),
    salt: salt,
    method: 'sha256'
  };
};

// validates a username
const validateName = function (username) {
  if (typeof username !== 'string' || username === '') {
    const err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_USER_INVALID_NAME.code;
    err.errorMessage = arangodb.errors.ERROR_USER_INVALID_NAME.message;

    throw err;
  }
};

// validates password
const validatePassword = function (password) {
  if (typeof password !== 'string') {
    const err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_USER_INVALID_PASSWORD.code;
    err.errorMessage = arangodb.errors.ERROR_USER_INVALID_PASSWORD.message;

    throw err;
  }

  if (password === 'ARANGODB_DEFAULT_ROOT_PASSWORD') {
    password = require('process').env.ARANGODB_DEFAULT_ROOT_PASSWORD || '';
  }

  return password;
};

// returns the users collection
const getStorage = function () {
  if (db._name() !== '_system') {
    const err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_FORBIDDEN.code;
    err.errorMessage = 'users can only be used in _system database';
    throw err;
  }

  const users = db._collection('_users');

  if (users === null) {
    const err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code;
    err.errorMessage = 'collection _users not found';
    throw err;
  }

  return users;
};

// creates a new user
exports.save = function (username, password, active, userData, changePassword) {
  const users = getStorage();

  if (password === null || password === undefined) {
    password = '';
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

  const user = users.firstExample({
    user: username
  });

  if (user !== null) {
    const err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_USER_DUPLICATE.code;
    err.errorMessage = arangodb.errors.ERROR_USER_DUPLICATE.message;
    throw err;
  }

  const data = {
    user: username,
    databases: {},
    configData: {},
    userData: userData || {},
    authData: {
      simple: hashPassword(password),
      active: Boolean(active),
      changePassword: Boolean(changePassword)
    }
  };

  const doc = users.save(data);

  // not exports.reload() as this is an abstract method...
  require('@arangodb/users').reload();

  return convertToLegacyFormat(users.document(doc._id));
};

// replaces an existing user
exports.replace = function (username, password, active, userData, changePassword) {
  const users = getStorage();

  if (password === null || password === undefined) {
    password = '';
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

  const user = users.firstExample({
    user: username
  });

  if (user === null) {
    const err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_USER_NOT_FOUND.code;
    err.errorMessage = arangodb.errors.ERROR_USER_NOT_FOUND.message;
    throw err;
  }

  const data = {
    user: username,
    databases: user.databases,
    configData: user.configData,
    userData: userData || {},
    authData: {
      simple: hashPassword(password),
      active: Boolean(active),
      changePassword: Boolean(changePassword)
    }
  };

  const doc = users.replace(user, data);

  // not exports.reload() as this is an abstract method...
  require('@arangodb/users').reload();

  return convertToLegacyFormat(users.document(doc._id));
};

// updates an existing user
exports.update = function (username, password, active, userData, changePassword) {
  const users = getStorage();

  // validate input
  validateName(username);

  if (password !== undefined) {
    password = validatePassword(password);
  }

  const user = users.firstExample({
    user: username
  });

  if (user === null) {
    const err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_USER_NOT_FOUND.code;
    err.errorMessage = arangodb.errors.ERROR_USER_NOT_FOUND.message;
    throw err;
  }

  const data = shallowCopy(user);

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
  require('@arangodb/users').reload();

  return convertToLegacyFormat(users.document(user._id));
};

// deletes an existing user
exports.remove = function (username) {
  const users = getStorage();

  // validate input
  validateName(username);

  const user = users.firstExample({
    user: username
  });

  if (user === null) {
    const err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_USER_NOT_FOUND.code;
    err.errorMessage = arangodb.errors.ERROR_USER_NOT_FOUND.message;
    throw err;
  }

  // not exports.reload() as this is an abstract method...
  require('@arangodb/users').reload();

  users.remove(user);
};

// gets an existing user
exports.document = function (username) {
  const users = getStorage();

  // validate name
  validateName(username);

  const user = users.firstExample({
    user: username
  });

  if (user === null) {
    const err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_USER_NOT_FOUND.code;
    err.errorMessage = arangodb.errors.ERROR_USER_NOT_FOUND.message;
    throw err;
  }

  return convertToLegacyFormat(user);
};

exports.exists = function (username) {
  try {
    exports.document(username);
    return true;
  } catch (e) {
    if (e.errorNum === arangodb.errors.ERROR_USER_NOT_FOUND.code) {
      return false;
    }
    throw e;
  }
};

// checks whether a combination of username / password is valid.
exports.isValid = function (username, password) {
  const users = getStorage();

  // validate name
  validateName(username);

  const user = users.firstExample({
    user: username
  });

  if (user === null || !user.authData.active) {
    return false;
  }

  // penalize the call
  internal.sleep(Math.random());

  const hash = crypto[user.authData.simple.method](user.authData.simple.salt + password);
  return crypto.constantEquals(user.authData.simple.hash, hash);
};

// gets all existing users
exports.all = function () {
  const users = getStorage();

  return users.all().toArray().map(convertToLegacyFormat);
};

// reloads the user authentication data
exports.reload = function () {
  internal.reloadAuth();

  if (require('@arangodb/cluster').isCoordinator()) {
    // Tell the agency about this reload, such that all other coordinators
    // reload as well. This is important because most calls to this
    // function here come from actual changes in the collection _users.
    let UserVersion;
    let done = false;

    while (!done) {
      try {
        UserVersion = ArangoAgency.get('Sync/UserVersion');
        UserVersion = UserVersion.arango.Sync.UserVersion;
      } catch (err) {
        break;
      }
      try {
        done = ArangoAgency.cas('Sync/UserVersion', UserVersion, UserVersion + 1);
      } catch (err2) {
        break;
      }
    }
  }
};

// sets a password-change token
exports.setPasswordToken = function (username, token) {
  const users = getStorage();

  const user = users.firstExample({
    user: username
  });

  if (user === null) {
    return null;
  }

  if (token === null || token === undefined) {
    token = internal.genRandomAlphaNumbers(50);
  }

  users.update(user, {
    authData: {
      passwordToken: token
    }
  });

  return token;
};

// checks the password-change token
exports.userByToken = function (token) {
  const users = getStorage();

  return users.firstExample({
    'authData.passwordToken': token
  });
};

// checks the password-change token
exports.changePassword = function (token, password) {
  const users = getStorage();

  const user = users.firstExample({
    'authData.passwordToken': token
  });

  if (user === null) {
    return false;
  }

  password = validatePassword(password);

  const authData = shallowCopy(user).authData;

  delete authData.passwordToken;
  authData.simple = hashPassword(password);
  authData.changePassword = false;

  users.update(user, {
    authData: authData
  });

  // not exports.reload() as this is an abstract method...
  require('@arangodb/users').reload();

  return true;
};

// changes the allowed databases
exports.grantDatabase = function (username, database, type) {
  const users = getStorage();

  if (type === undefined) {
    type = 'rw';
  } else if (type !== 'rw' && type !== 'ro') {
    const err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_BAD_PARAMETER.code;
    err.errorMessage = "expecting access type 'rw' or 'ro'";
    throw err;
  }

  // validate name
  validateName(username);

  const user = users.firstExample({
    user: username
  });

  if (user === null) {
    const err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_USER_NOT_FOUND.code;
    err.errorMessage = arangodb.errors.ERROR_USER_NOT_FOUND.message;
    throw err;
  }

  let databases = user.databases || {};
  databases[database] = type;

  users.update(user, { databases: databases });

  // not exports.reload() as this is an abstract method...
  require('@arangodb/users').reload();

  return databases;
};

// changes the allowed databases
exports.revokeDatabase = function (username, database) {
  const users = getStorage();

  // validate name
  validateName(username);

  const user = users.firstExample({
    user: username
  });

  if (user === null) {
    const err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_USER_NOT_FOUND.code;
    err.errorMessage = arangodb.errors.ERROR_USER_NOT_FOUND.message;
    throw err;
  }

  let databases = user.databases || {};
  databases[database] = 'none';

  users.update(user, { databases: databases }, false, false);

  // not exports.reload() as this is an abstract method...
  require('@arangodb/users').reload();

  return databases;
};

// create/update (value != null) or delete (value == null)
exports.updateConfigData = function (username, key, value) {
  const users = getStorage();

  validateName(username);

  const user = users.firstExample({
    user: username
  });

  if (user === null) {
    const err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_USER_NOT_FOUND.code;
    err.errorMessage = arangodb.errors.ERROR_USER_NOT_FOUND.message;
    throw err;
  }

  if (value === undefined) {
    value = null;
  }

  var options = user.configData;

  if (key === undefined || key === null) {
    var data = shallowCopy(user);
    data.configData = {};
    users.replace(user, data);
  } else {
    options[key] = value;
    users.update(user, { configData: options }, false, false);
  }
};

// one config data (key != null) or all (key == null)    
exports.configData = function (username, key) {
  const users = getStorage();

  validateName(username);

  const user = users.firstExample({
    user: username
  });

  if (user === null) {
    const err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_USER_NOT_FOUND.code;
    err.errorMessage = arangodb.errors.ERROR_USER_NOT_FOUND.message;
    throw err;
  }

  if (key === undefined || key === null) {
    return user.configData;
  } else {
    return user.configData[key];
  }
};

// one db permission data (key != null) or all (key == null)    
exports.permission = function (username, key) {
  const users = getStorage();

  validateName(username);

  const user = users.firstExample({
    user: username
  });

  if (user === null) {
    const err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_USER_NOT_FOUND.code;
    err.errorMessage = arangodb.errors.ERROR_USER_NOT_FOUND.message;
    throw err;
  }

  if (key === undefined || key === null) {
    let databases = user.databases;
    let result = {};

    if (databases.hasOwnProperty('*')) {
      const p = databases['*'];
      const l = db._databases();

      for (let k = 0; k < l.length; ++k) {
        var dbname = l[k];
        result[dbname] = p;
      }
    }

    for (let k in databases) {
      if (k !== '*' && databases.hasOwnProperty(k)) {
        result[k] = databases[k];
      }
    }

    return result;
  } else {
    if (key === '*') {
      return user.databases[key];
    } else {
      if (user.databases.hasOwnProperty(key)) {
        return user.databases[key];
      }

      if (user.databases.hasOwnProperty('*')) {
        return user.databases['*'];
      }

      return '';
    }
  }
};
