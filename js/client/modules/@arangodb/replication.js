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

var internal = require('internal');
var arangosh = require('@arangodb/arangosh');

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
  var db = internal.db;

  var requestResult = db._connection.GET('/_api/replication/logger-first-tick');
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
  if (global) {
    append += appendChar(append) + 'global=true';
  }

  var requestResult = internal.db._connection.PUT('/_api/replication/applier-start' + append, '');
  arangosh.checkRequestResult(requestResult);

  return requestResult;
}

applier.start = function (initialTick, barrierId) { return applierStart(false, initialTick, barrierId); };
globalApplier.start = function (initialTick, barrierId) { return applierStart(true, initialTick, barrierId); };

// //////////////////////////////////////////////////////////////////////////////
// / @brief stops the replication applier
// //////////////////////////////////////////////////////////////////////////////

function applierShutdown(global) {
  var append = '';
  if (global) {
    append += appendChar(append) + 'global=true';
  }

  var requestResult = internal.db._connection.PUT('/_api/replication/applier-stop' + append, '');
  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

applier.stop = applier.shutdown = function () { return applierShutdown(false); };
globalApplier.stop = applier.shutdown = function () { return applierShutdown(true); };

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the replication applier state
// //////////////////////////////////////////////////////////////////////////////

function applierState(global) {
  var append = '';
  if (global) {
    append += appendChar(append) + 'global=true';
  }

  var requestResult = internal.db._connection.GET('/_api/replication/applier-state' + append);
  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

applier.state = function () { return applierState(false); };
globalApplier.state = function () { return applierState(true); };

// //////////////////////////////////////////////////////////////////////////////
// / @brief stop the replication applier state and "forget" all state
// //////////////////////////////////////////////////////////////////////////////

function applierForget(global) {
  var append = '';
  if (global) {
    append += appendChar(append) + 'global=true';
  }

  var requestResult = internal.db._connection.DELETE('/_api/replication/applier-state' + append);
  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

applier.forget = function () { return applierForget(false); };
globalApplier.forget = function () { return applierForget(true); };

// //////////////////////////////////////////////////////////////////////////////
// / @brief configures the replication applier
// //////////////////////////////////////////////////////////////////////////////

function applierProperties(global, config) {
  var append = '';
  if (global) {
    append += appendChar(append) + 'global=true';
  }

  var requestResult;
  if (config === undefined) {
    requestResult = internal.db._connection.GET('/_api/replication/applier-config' + append);
  } else {
    requestResult = internal.db._connection.PUT('/_api/replication/applier-config' + append,
      JSON.stringify(config));
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

  if (!config.hasOwnProperty('progress')) {
    config.progress = true;
  }

  internal.sleep(1);
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
    if (iterations < 6) {
      internal.sleep(2);
    } else {
      internal.sleep(3);
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

var sync = function (config) {
  const body = JSON.stringify(config || {});
  const headers = {
    'X-Arango-Async': 'store'
  };

  const requestResult = internal.db._connection.PUT_RAW('/_api/replication/sync', body, headers);
  arangosh.checkRequestResult(requestResult);

  if (config.async) {
    return requestResult.headers['x-arango-async-id'];
  }

  return waitForResult(config, requestResult.headers['x-arango-async-id']);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief performs a one-time synchronization with a remote endpoint, for
// / a single collection
// //////////////////////////////////////////////////////////////////////////////

var syncCollection = function (collection, config) {
  config = config || {};
  config.restrictType = 'include';
  config.restrictCollections = [collection];
  config.includeSystem = true;

  return sync(config);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief performs a one-time synchronization with a remote endpoint
// //////////////////////////////////////////////////////////////////////////////

var syncGlobal = function (config) {
  const body = JSON.stringify(config || {});
  const headers = {
    'X-Arango-Async': 'store'
  };

  const requestResult = internal.db._connection.PUT_RAW('/_api/replication/sync?global=true', body, headers);
  arangosh.checkRequestResult(requestResult);

  if (config.async) {
    return requestResult.headers['x-arango-async-id'];
  }

  return waitForResult(config, requestResult.headers['x-arango-async-id']);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief sets up the replication (all-in-one function for initial
// / synchronization and continuous replication)
// //////////////////////////////////////////////////////////////////////////////

var setupReplication = function (config) {
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

  const db = internal.db;

  const body = JSON.stringify(config);
  const headers = {
    'X-Arango-Async': 'store'
  };

  const requestResult = db._connection.PUT_RAW('/_api/replication/make-slave', body, headers);
  arangosh.checkRequestResult(requestResult);

  if (config.async) {
    return requestResult.headers['x-arango-async-id'];
  }

  return waitForResult(config, requestResult.headers['x-arango-async-id']);
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
exports.applier = applier;
exports.globalApplier = globalApplier;
exports.sync = sync;
exports.syncCollection = syncCollection;
exports.syncGlobal = syncGlobal;
exports.setupReplication = setupReplication;
exports.getSyncResult = getSyncResult;
exports.serverId = serverId;
