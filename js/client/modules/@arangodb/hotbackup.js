/* jshint strict: false */

// //////////////////////////////////////////////////////////////////////////////
// / @brief ArangoDatabase
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2013 triagens GmbH, Cologne, Germany
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
// / @author Wilfried Goesgnes
// / @author Copyright 2019, ArangoDB Inc, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////


var internal = require('internal');
var arangosh = require('@arangodb/arangosh');

var ArangoError = require('@arangodb').ArangoError;

// //////////////////////////////////////////////////////////////////////////////
// / @brief get a list of the hot backups on the server
// //////////////////////////////////////////////////////////////////////////////

exports.get =  function () {
  let reply = internal.db._connection.POST('_admin/hotbackup/list', null);
  if (!reply.error && reply.code === 200 && reply.result.hasOwnProperty('hotbackups')) {
    return reply.result.hotbackups;
  }
  throw new ArangoError(reply);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief trigger a new hot backu
// //////////////////////////////////////////////////////////////////////////////

exports.create = function (userString = undefined) {
  let postData = {};
  if (userString !== undefined) {
    postData['userString'] = userString;
  }
  let reply = internal.db._connection.POST('_admin/hotbackup/create', postData);
  if (!reply.error && reply.code === 200) {
    return reply.result;
  }
  throw new ArangoError(reply);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief restore a hot backup to the server
// //////////////////////////////////////////////////////////////////////////////

exports.restore = function(restoreBackupName) {
  let reply = internal.db._connection.POST('_admin/hotbackup/restore', { directory: restoreBackupName });
  if (!reply.error && reply.code === 200) {
    return reply.result;
  }
  throw new ArangoError(reply);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief deletes a hot backup on the server
// //////////////////////////////////////////////////////////////////////////////

exports.delete = function(deleteBackupName) {
  let reply = internal.db._connection.DELETE('_admin/hotbackup/create', { directory: deleteBackupName });
  if (!reply.error && reply.code === 200) {
    return reply.result;
  }
  throw new ArangoError(reply);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief stores a hot backup on the server
// //////////////////////////////////////////////////////////////////////////////

exports.store = function(storeBackupName) {
  let reply = internal.db._connection.GET('_admin/hotbackup/store', { directory: storeBackupName });
  if (!reply.error && reply.code === 200) {
    return reply.result;
  }
  throw new ArangoError(reply);
};


// //////////////////////////////////////////////////////////////////////////////
// / @brief retrieves a hot backup from the server
// //////////////////////////////////////////////////////////////////////////////

exports.retrieve = function(backupName) {
  let reply = internal.db._connection.POST('_admin/hotbackup/retrieve', { directory: backupName });
  if (!reply.error && reply.code === 200) {
    return reply.result;
  }
  throw new ArangoError(reply);
};


// //////////////////////////////////////////////////////////////////////////////
// / @brief fetches the hot backup policy
// //////////////////////////////////////////////////////////////////////////////

exports.getPolicy = function() {
  let reply = internal.db._connection.GET('_admin/hotbackup/policy');
  if (!reply.error && reply.code === 200) {
    return reply.result;
  }
  throw new ArangoError(reply);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief sets a hot backup policy
// //////////////////////////////////////////////////////////////////////////////

exports.setPolicy = function(policy) {
  let reply = internal.db._connection.POST('_admin/hotbackup/policy', {policy: policy });
  if (!reply.error && reply.code === 200) {
    return reply.result;
  }
  throw new ArangoError(reply);
};
