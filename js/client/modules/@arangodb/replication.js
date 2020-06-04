'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief Replication management
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2012 triagens GmbH, Cologne, Germany
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
// / @author Copyright 2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const internal = require('internal');
const arangosh = require('@arangodb/arangosh');
const rpc = require('@arangodb/replication-common');

let logger = {};
let applier = {};
let globalApplier = {};
    
function appendChar(append) {
  return (append === '' ? '?' : '&');
}

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
// / @brief return the tick ranges that can be provided by the replication logger
// //////////////////////////////////////////////////////////////////////////////

logger.tickRanges = function () {
  var db = internal.db;

  var requestResult = db._connection.GET('/_api/replication/logger-tick-ranges');
  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the first tick that can be provided by the replication logger
// //////////////////////////////////////////////////////////////////////////////

logger.firstTick = function () {
  var requestResult = internal.db._connection.GET('/_api/replication/logger-first-tick');
  arangosh.checkRequestResult(requestResult);

  return requestResult.firstTick;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief starts the replication applier
// //////////////////////////////////////////////////////////////////////////////

function applierStart(global, initialTick, barrierId) {
  var append = '';
  if (initialTick !== undefined) {
    append = appendChar(append) + 'from=' + encodeURIComponent(initialTick);
  }
  if (barrierId !== undefined) {
    append += appendChar(append) + 'barrierId=' + encodeURIComponent(barrierId);
  }

  var url;
  if (global) {
    url = '/_db/_system/_api/replication/applier-start' + append + appendChar(append) + 'global=true';
  } else {
    url = '/_api/replication/applier-start' + append;
  }
  var requestResult = internal.db._connection.PUT(url, '');
  arangosh.checkRequestResult(requestResult);
  return requestResult;
}

applier.start = function (initialTick, barrierId) { return applierStart(false, initialTick, barrierId); };
globalApplier.start = function (initialTick, barrierId) { return applierStart(true, initialTick, barrierId); };

// //////////////////////////////////////////////////////////////////////////////
// / @brief stops the replication applier
// //////////////////////////////////////////////////////////////////////////////

function applierStop(global) {
  var url;
  if (global) {
    url = '/_db/_system/_api/replication/applier-stop?global=true';
  } else {
    url = '/_api/replication/applier-stop';
  }

  var requestResult = internal.db._connection.PUT(url, '');
  arangosh.checkRequestResult(requestResult);
  return requestResult;
};

applier.stop = function () { return applierStop(false); };
globalApplier.stop = function () { return applierStop(true); };

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the replication applier state
// //////////////////////////////////////////////////////////////////////////////

function applierState(global) {
  var url;
  if (global) {
    url = '/_db/_system/_api/replication/applier-state?global=true';
  } else {
    url = '/_api/replication/applier-state';
  }

  var requestResult = internal.db._connection.GET(url);
  arangosh.checkRequestResult(requestResult);
  return requestResult;
};

applier.state = function () { return applierState(false); };
globalApplier.state = function () { return applierState(true); };

// //////////////////////////////////////////////////////////////////////////////
// / @brief return all replication applier states
// //////////////////////////////////////////////////////////////////////////////

function applierStateAll(global) {
  var url;
  if (global) {
    url = '/_db/_system/_api/replication/applier-state-all?global=true';
  } else {
    url = '/_api/replication/applier-state-all';
  }

  var requestResult = internal.db._connection.GET(url);
  arangosh.checkRequestResult(requestResult);
  return requestResult;
};

applier.stateAll= function () { return applierStateAll(false); };
globalApplier.stateAll = function () { return applierStateAll(true); };

// //////////////////////////////////////////////////////////////////////////////
// / @brief stop the replication applier state and "forget" all state
// //////////////////////////////////////////////////////////////////////////////

function applierForget(global) {
  var url;
  if (global) {
    url = '/_db/_system/_api/replication/applier-state?global=true';
  } else {
    url = '/_api/replication/applier-state';
  }

  var requestResult = internal.db._connection.DELETE(url);
  arangosh.checkRequestResult(requestResult);
  return requestResult;
};

applier.forget = function () { return applierForget(false); };
globalApplier.forget = function () { return applierForget(true); };

// //////////////////////////////////////////////////////////////////////////////
// / @brief configures the replication applier
// //////////////////////////////////////////////////////////////////////////////

function applierProperties(global, config) {
  var url;
  if (global) {
    url = '/_db/_system/_api/replication/applier-config?global=true';
  } else {
    url = '/_api/replication/applier-config';
  }

  var requestResult;
  if (config === undefined) {
    requestResult = internal.db._connection.GET(url);
  } else {
    requestResult = internal.db._connection.PUT(url, config);
  }
  arangosh.checkRequestResult(requestResult);
  return requestResult;
};

applier.properties = function (config) { return applierProperties(false, config); };
globalApplier.properties = function (config) { return applierProperties(true, config); };

// //////////////////////////////////////////////////////////////////////////////
// / @brief helper function for fetching the result of an async job
// //////////////////////////////////////////////////////////////////////////////

var waitForResult = function (config, id) {
  const db = internal.db;

  let sleepTime = 0.05;

  if (!config.hasOwnProperty('progress')) {
    config.progress = true;
  }

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

    if (config.progress && iterations % 3 === 0) {
      try {
        var progress = applier.state().state.progress;
        var msg = progress.time + ': ' + progress.message;
        internal.print('still synchronizing... last received status: ' + msg);
      } catch (err) {}
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
// / @brief sets up the replication (all-in-one function for initial
// / synchronization and continuous replication)
// //////////////////////////////////////////////////////////////////////////////

var setup = function (global, config) {
  var url;
  if (global) {
    url = '/_db/_system/_api/replication/make-slave?global=true';
  } else {
    url = '/_api/replication/make-slave';
  }

  config = config || { };
  if (!config.hasOwnProperty('autoStart')) {
    config.autoStart = true;
  }
  if (!config.hasOwnProperty('includeSystem')) {
    config.includeSystem = true;
  }
  if (!config.hasOwnProperty('verbose')) {
    config.verbose = false;
  }

  const headers = {
    'X-Arango-Async': 'store'
  };

  const requestResult = internal.db._connection.PUT_RAW(url, config, headers);
  arangosh.checkRequestResult(requestResult);

  if (config.async) {
    return requestResult.headers['x-arango-async-id'];
  }

  return waitForResult(config, requestResult.headers['x-arango-async-id']);
};

var setupReplication = function (config) { return setup(false, config); };
var setupReplicationGlobal = function (config) { return setup(true, config); };

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
exports.applier = applier;
exports.globalApplier = globalApplier;
exports.sync = syncDatabase;
exports.syncGlobal = syncGlobal;
exports.syncCollection = syncCollection;
exports.setupReplication = setupReplication;
exports.setupReplicationGlobal = setupReplicationGlobal;
exports.getSyncResult = getSyncResult;
exports.serverId = serverId;
exports.compareTicks = rpc.compareTicks;
