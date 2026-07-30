'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// //////////////////////////////////////////////////////////////////////////////

const internal = require('internal');
const arangosh = require('@arangodb/arangosh');
const rpc = require('@arangodb/replication-common');

let logger = {};

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the replication logger state
// //////////////////////////////////////////////////////////////////////////////

logger.state = function () {
  var db = internal.db;

  var requestResult = db._connection.GET('/_api/replication/logger-state');
  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the last log tick
// //////////////////////////////////////////////////////////////////////////////

logger.lastLogTick = function (firstTick, lastTick) {
  var requestResult = internal.db._connection.GET(`/_api/replication/logger-last?tickStart=${firstTick}&tickEnd=${lastTick}`);
  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief helper function for fetching the result of an async job
// //////////////////////////////////////////////////////////////////////////////

var waitForResult = function (config, id) {
  const db = internal.db;

  let sleepTime = 0.05;

  internal.sleep(sleepTime);
  var iterations = 0;

  while (true) {
    const jobResult = db._connection.PUT('/_api/job/' + encodeURIComponent(id), '');
    try {
      arangosh.checkRequestResult(jobResult);
    } catch (err) {
      throw err;
    }

    if (jobResult.code !== 204) {
      return jobResult;
    }

    ++iterations;
    if (iterations > 6) {
      internal.sleep(sleepTime);
    } else {
      internal.sleep(sleepTime);
    }

  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief performs a one-time synchronization with a remote endpoint
// //////////////////////////////////////////////////////////////////////////////

var sync = function (global, config) {
  var url;
  if (global) {
    url = '/_db/_system/_api/replication/sync?global=true';
  } else {
    url = '/_api/replication/sync';
  }
  const headers = {
    'X-Arango-Async': 'store'
  };

  const requestResult = internal.db._connection.PUT_RAW(url, config || {}, headers);
  arangosh.checkRequestResult(requestResult);

  if (config.async) {
    return requestResult.headers['x-arango-async-id'];
  }

  return waitForResult(config, requestResult.headers['x-arango-async-id']);
};

var syncDatabase = function (config) { return sync(false, config); };
var syncGlobal = function (config) { return sync(true, config); };

// //////////////////////////////////////////////////////////////////////////////
// / @brief performs a one-time synchronization with a remote endpoint, for
// / a single collection
// //////////////////////////////////////////////////////////////////////////////

var syncCollection = function (collection, config) {
  config = config || {};
  config.restrictType = 'include';
  config.restrictCollections = [collection];
  config.includeSystem = true;

  return sync(false, config);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief queries the sync result status
// //////////////////////////////////////////////////////////////////////////////

var getSyncResult = function (id) {
  var db = internal.db;

  var requestResult = db._connection.PUT_RAW('/_api/job/' + encodeURIComponent(id), '');
  arangosh.checkRequestResult(requestResult);

  if (requestResult.headers.hasOwnProperty('x-arango-async-id')) {
    return JSON.parse(requestResult.body);
  }

  return false;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief fetches a server's id
// //////////////////////////////////////////////////////////////////////////////

var serverId = function () {
  var db = internal.db;

  var requestResult = db._connection.GET('/_api/replication/server-id');

  arangosh.checkRequestResult(requestResult);

  return requestResult.serverId;
};

exports.logger = logger;
exports.sync = syncDatabase;
exports.syncGlobal = syncGlobal;
exports.syncCollection = syncCollection;
exports.getSyncResult = getSyncResult;
exports.serverId = serverId;
exports.compareTicks = rpc.compareTicks;
