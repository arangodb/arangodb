/* jshint strict: false */
/* global ArangoAgency */

'use strict';

////////////////////////////////////////////////////////////////////////////////
// @brief User management
//
// @file
//
// DISCLAIMER
//
// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
//
// Licensed under the Apache License, Version 2.0 (the "License")
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
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
// @author Simon Grätzer
// @author Copyright 2012-2017, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const internal = require('internal'); // OK: reloadAuth
const arangodb = require("@arangodb");

// //////////////////////////////////////////////////////////////////////////////
// / @brief constructor
// //////////////////////////////////////////////////////////////////////////////

var ArangoUsers = internal.ArangoUsers;
exports.save = ArangoUsers.save;
exports.replace = ArangoUsers.replace;
exports.update = ArangoUsers.update;
exports.remove = ArangoUsers.remove;
exports.document = ArangoUsers.document;
exports.reload = ArangoUsers.reload;
exports.grantDatabase = ArangoUsers.grantDatabase;
exports.revokeDatabase = ArangoUsers.revokeDatabase;
exports.grantCollection = ArangoUsers.grantCollection;
exports.revokeCollection = ArangoUsers.revokeCollection;
exports.updateConfigData = ArangoUsers.updateConfigData;
exports.revokeCollection = ArangoUsers.revokeCollection;
exports.updateConfigData = ArangoUsers.updateConfigData;
exports.configData = ArangoUsers.configData;
exports.permission = ArangoUsers.permission;
exports.currentUser = ArangoUsers.currentUser;
exports.isAuthActive = ArangoUsers.isAuthActive;
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