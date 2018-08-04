/* jshint strict: false */

// /////////////////////////////////////////////////////////////////////////////
// @brief User management
//
// DISCLAIMER
//
// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
//
// Licensed under the Apache License, Version 2.0 (the "License")
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Copyright holder is triAGENS GmbH, Cologne, Germany
//
// @author Jan Steemann
// @author Copyright 2012-2014, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const internal = require('internal');
const arangodb = require('@arangodb');
const arangosh = require('@arangodb/arangosh');

// creates a new user
exports.save = function (user, passwd, active, extra, changePassword) {
  let db = internal.db;

  let uri = '_api/user/';
  let data = {user: user};

  if (passwd !== undefined) {
    data.passwd = passwd;
  }

  if (active !== undefined) {
    data.active = active;
  }

  if (extra !== undefined) {
    data.extra = extra;
  }

  if (changePassword !== undefined) {
    data.changePassword = changePassword;
  }

  let requestResult = db._connection.POST(uri, data);
  return arangosh.checkRequestResult(requestResult);
};

// replaces an existing user
exports.replace = function (user, passwd, active, extra, changePassword) {
  var db = internal.db;

  var uri = '_api/user/' + encodeURIComponent(user);
  var data = {
    passwd: passwd,
    active: active,
    extra: extra,
    changePassword: changePassword
  };

  var requestResult = db._connection.PUT(uri, data);
  return arangosh.checkRequestResult(requestResult);
};

// updates an existing user
exports.update = function (user, passwd, active, extra, changePassword) {
  var db = internal.db;

  var uri = '_api/user/' + encodeURIComponent(user);
  var data = {};

  if (passwd !== undefined) {
    data.passwd = passwd;
  }

  if (active !== undefined) {
    data.active = active;
  }

  if (extra !== undefined) {
    data.extra = extra;
  }

  if (changePassword !== undefined) {
    data.changePassword = changePassword;
  }

  var requestResult = db._connection.PATCH(uri, data);
  return arangosh.checkRequestResult(requestResult);
};

// deletes an existing user
exports.remove = function (user) {
  var db = internal.db;

  var uri = '_api/user/' + encodeURIComponent(user);

  var requestResult = db._connection.DELETE(uri);
  arangosh.checkRequestResult(requestResult);
};

// gets an existing user
exports.document = function (user) {
  var db = internal.db;

  var uri = '_api/user/' + encodeURIComponent(user);

  var requestResult = db._connection.GET(uri);
  return arangosh.checkRequestResult(requestResult);
};

// checks whether a combination of username / password is valid.
exports.isValid = function (user, password) {
  var db = internal.db;

  var uri = '_api/user/' + encodeURIComponent(user);
  var data = { passwd: password };

  var requestResult = db._connection.POST(uri, data);

  if (requestResult.error !== undefined && requestResult.error) {
    if (requestResult.errorNum === arangodb.errors.ERROR_USER_NOT_FOUND.code) {
      return false;
    }

    return arangosh.checkRequestResult(requestResult);
  }

  return requestResult.result;
};

// gets all existing users
exports.all = function () {
  var db = internal.db;

  var uri = '_api/user';

  var requestResult = db._connection.GET(uri);
  return arangosh.checkRequestResult(requestResult).result;
};

// reloads the user authentication data
exports.reload = function () {
  var db = internal.db;

  var requestResult = db._connection.GET('_admin/auth/reload');
  arangosh.checkRequestResult(requestResult);
};

// changes the allowed databases
exports.grantDatabase = function (username, database, type) {
  if (type === undefined) {
    type = 'rw';
  }

  var db = internal.db;
  var uri = '_api/user/' + encodeURIComponent(username)
  + '/database/' + encodeURIComponent(database);
  var data = { grant: type };

  var requestResult = db._connection.PUT(uri, data);

  return arangosh.checkRequestResult(requestResult).result;
};

// changes the allowed databases
exports.revokeDatabase = function (username, database) {
  var db = internal.db;
  var uri = '_api/user/' + encodeURIComponent(username)
  + '/database/' + encodeURIComponent(database);
  var requestResult = db._connection.DELETE(uri);

  return arangosh.checkRequestResult(requestResult).result;
};

// changes the collection access level
exports.grantCollection = function (username, database, collection, type) {
  if (type === undefined) {
    type = 'rw';
  }

  var db = internal.db;
  var uri = '_api/user/' + encodeURIComponent(username)
  + '/database/' + encodeURIComponent(database) + '/'
  + encodeURIComponent(collection);
  var data = { grant: type };

  var requestResult = db._connection.PUT(uri, data);

  return arangosh.checkRequestResult(requestResult).result;
};

// changes the collection access level
exports.revokeCollection = function (username, database, collection) {
  var db = internal.db;
  var uri = '_api/user/' + encodeURIComponent(username)
  + '/database/' + encodeURIComponent(database) + '/'
  + encodeURIComponent(collection);
  var requestResult = db._connection.DELETE(uri);

  return arangosh.checkRequestResult(requestResult).result;
};

// create/update (value != null) or delete (value == null)
exports.updateConfigData = function (username, key, value) {
  var db = internal.db;
  var requestResult;
  var uri;

  if (key === undefined || key === null) {
    uri = '_api/user/' + encodeURIComponent(username)
      + '/config';

    requestResult = db._connection.DELETE(uri);
  } else {
    uri = '_api/user/' + encodeURIComponent(username)
    + '/config/' + encodeURIComponent(key);

    var data = { value: value };
    requestResult = db._connection.PUT(uri, data);
  }

  arangosh.checkRequestResult(requestResult);
};

// one config data (key != null) or all (key == null)
exports.configData = function (username, key) {
  var db = internal.db;
  var requestResult;
  var uri;

  if (key === undefined || key === null) {
    uri = '_api/user/' + encodeURIComponent(username)
      + '/config';

    requestResult = db._connection.GET(uri);
  } else {
    uri = '_api/user/' + encodeURIComponent(username)
    + '/config/' + encodeURIComponent(key);

    requestResult = db._connection.GET(uri);
  }

  return arangosh.checkRequestResult(requestResult).result;
};

// one db permission data (key != null) or all (key == null)
exports.permission = function (username, dbName, coll) {
  let db = internal.db;
  let requestResult;
  let uri;

  if (dbName === undefined || dbName === null) {
    uri = '_api/user/' + encodeURIComponent(username)
      + '/database';

    requestResult = db._connection.GET(uri);
  } else {
    uri = '_api/user/' + encodeURIComponent(username)
    + '/database/' + encodeURIComponent(dbName);
    if (coll) {
      uri += '/' + encodeURIComponent(coll);
    }
    requestResult = db._connection.GET(uri);
  }

  return arangosh.checkRequestResult(requestResult).result;
};

// return the full list of permissions and collections
exports.permissionFull = function (username) {
  let db = internal.db;
  let uri  = '_api/user/' + encodeURIComponent(username)
      + '/database?full=true';
  let requestResult = db._connection.GET(uri);
  return arangosh.checkRequestResult(requestResult).result;
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

exports.currentUser = function() {
  return internal.arango.connectedUser();
};

exports.isAuthActive = function() {
  let active = false;
  try {
    let c = internal.db._collection("_users");
    c.properties(); // we need to access c to trigger the exception
  } catch(e) {
    active = true;
  }
  return active;
};
