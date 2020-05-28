/* jshint strict: false */
/* global: arango */

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
const sleep = require("internal").sleep;

var ArangoError = require('@arangodb').ArangoError;

// //////////////////////////////////////////////////////////////////////////////
// / @brief get a list of the hot backups on the server
// //////////////////////////////////////////////////////////////////////////////

exports.get =  function () {
  let reply = internal.db._connection.POST('_admin/backup/list', null);
  if (!reply.error && reply.code === 200 && reply.result.hasOwnProperty('list')) {
    return reply.result.list;
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
  let reply = internal.db._connection.POST('_admin/backup/create', postData);
  if (!reply.error && reply.code === 201) {
    return reply.result;
  }
  throw new ArangoError(reply);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief restore a hot backup to the server
// //////////////////////////////////////////////////////////////////////////////

const waitForRestart = (maxWait, originalUptime) => {
  if (maxWait === 0.0 || maxWait === undefined || maxWait === null || 
      isNaN(parseFloat(maxWait))) {
    return;
  }
  
  let start = Date.now();
  let originalUptimeKeys = Object.keys(originalUptime);
  while (((Date.now() - start) / 1000) < maxWait) {
    let currentUptime = this.getUptime();
    let keys = Object.keys(currentUptime);
    if (keys.length !== originalUptimeKeys ) {
      try {
        internal.db._connection.reconnect(this.instanceInfo.endpoint, '_system', 'root', '');
      }
      catch(x) {
        this.print(".");
        
        sleep(1.0);
        continue;
      }
    }
    let newer = true;
    keys.forEach(key => {
      if (!originalUptime.hasOwnProperty(key)) {
        newer = false;
      }
      if (originalUptime[key] < currentUptime[key]) {
        newer = false;
      }
    });
    
    if (newer) {
      try {
        internal.db._connection.reconnect(this.instanceInfo.endpoint, '_system', 'root', '');
        this.print("reconnected");
        return this.results.restoreHotBackup.status;
      }
      catch(x) {
        sleep(1.0);
        this.print(",");
        continue;
      }
    }
    sleep(1.0);
  }
  throw ("Arangod didn't come back up in the expected timeframe!");
};

exports.restore = function(restoreBackupName, maxWait) {
  let originalUptime = this.getUptime();
  let reply = internal.db._connection.POST('_admin/backup/restore', { id: restoreBackupName });
  if (!reply.error && reply.code === 200) {
    waitForRestart(maxWait, originalUptime);
    return reply.result;
  }
  throw new ArangoError(reply);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief deletes a hot backup on the server
// //////////////////////////////////////////////////////////////////////////////

exports.delete = function(deleteBackupName) {
  let reply = internal.db._connection.POST('_admin/backup/delete', { id: deleteBackupName });
  if (!reply.error && reply.code === 200) {
    return reply.result;
  }
  throw new ArangoError(reply);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief stores a hot backup on the server
// //////////////////////////////////////////////////////////////////////////////

exports.upload = function(uploadBackupName, remoteRepository, config) {
  let reply = internal.db._connection.POST('_admin/backup/upload', 
    { id: uploadBackupName,
      remoteRepository,
      config });
  if (!reply.error && reply.code === 202) {
    return reply.result;
  }
  throw new ArangoError(reply);
};


// //////////////////////////////////////////////////////////////////////////////
// / @brief track progress of an upload
// //////////////////////////////////////////////////////////////////////////////

exports.uploadProgress = function(uploadId) {
  let reply = internal.db._connection.POST('_admin/backup/upload', 
    { uploadId: uploadId });
  if (!reply.error && reply.code === 200) {
    return reply.result;
  }
  throw new ArangoError(reply);
};


// //////////////////////////////////////////////////////////////////////////////
// / @brief retrieves a hot backup from the server
// //////////////////////////////////////////////////////////////////////////////

exports.download = function(backupName, remoteRepository, config) {
  let reply = internal.db._connection.POST('_admin/backup/download',
    { id: backupName,
      remoteRepository,
      config });
  if (!reply.error && reply.code === 202) {
    return reply.result;
  }
  throw new ArangoError(reply);
};


// //////////////////////////////////////////////////////////////////////////////
// / @brief track progress of a download
// //////////////////////////////////////////////////////////////////////////////

exports.downloadProgress = function(downloadId) {
  let reply = internal.db._connection.POST('_admin/backup/download', 
    { downloadId: downloadId });
  if (!reply.error && reply.code === 200) {
    return reply.result;
  }
  throw new ArangoError(reply);
};


