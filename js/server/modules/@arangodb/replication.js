/* global ArangoServerState, GLOBAL_REPLICATION_APPLIER_START, GLOBAL_REPLICATION_APPLIER_STOP, GLOBAL_REPLICATION_APPLIER_STATE, GLOBAL_REPLICATION_APPLIER_FORGET, GLOBAL_REPLICATION_APPLIER_CONFIGURE, GLOBAL_REPLICATION_SYNCHRONIZE, REPLICATION_APPLIER_START, REPLICATION_APPLIER_STOP, REPLICATION_APPLIER_STATE, REPLICATION_APPLIER_STATE_ALL, REPLICATION_APPLIER_FORGET, REPLICATION_APPLIER_CONFIGURE, REPLICATION_SYNCHRONIZE, GLOBAL_REPLICATION_APPLIER_FAILOVER_ENABLED */
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
const rpc = require('@arangodb/replication-common');
const ERRORS = internal.errors;
const endpointToURL = require('@arangodb/cluster').endpointToURL;

var request;
if (ArangoServerState.role() === 'PRIMARY') {
  request = require('@arangodb/request').clusterRequest;
} else {
  request = require('@arangodb/request').request;
}

let logger = { };
let applier = { };
let globalApplier = { };

// / @brief return the replication logger state
logger.state = function () {
  return internal.getStateReplicationLogger();
};

// / @brief return the tick ranges provided by the replication logger
logger.tickRanges = function () {
  return internal.tickRangesReplicationLogger();
};

// / @brief return the first tick that can be provided by the replication logger
logger.firstTick = function () {
  return internal.firstTickReplicationLogger();
};

// / @brief starts the replication applier
applier.start = function (initialTick, barrierId) {
  if (initialTick === undefined) {
    return REPLICATION_APPLIER_START();
  }

  return REPLICATION_APPLIER_START(initialTick, barrierId);
};

// / @brief shuts down the replication applier
applier.stop = function () { return REPLICATION_APPLIER_STOP(); };

// / @brief return the replication applier state
applier.state = function () { return REPLICATION_APPLIER_STATE(); };

// / @brief return the replication applier state of all dbs
applier.stateAll = function () { return REPLICATION_APPLIER_STATE_ALL(); };

// / @brief stop the applier and "forget" all configuration
applier.forget = function () { return REPLICATION_APPLIER_FORGET(); };

// / @brief returns the configuration of the replication applier
applier.properties = function (config) {
  if (config === undefined) {
    return REPLICATION_APPLIER_CONFIGURE();
  }

  return REPLICATION_APPLIER_CONFIGURE(config);
};

// / @brief starts the global replication applier
globalApplier.start = function (initialTick, barrierId) {
  if (initialTick === undefined) {
    return GLOBAL_REPLICATION_APPLIER_START();
  }

  return GLOBAL_REPLICATION_APPLIER_START(initialTick, barrierId);
};

// / @brief shuts down the global replication applier
globalApplier.stop = function () { return GLOBAL_REPLICATION_APPLIER_STOP(); };

// / @brief return the global replication applier state
globalApplier.state = function () { return GLOBAL_REPLICATION_APPLIER_STATE(); };

// / @brief stop the global applier and "forget" all configuration
globalApplier.forget = function () { return GLOBAL_REPLICATION_APPLIER_FORGET(); };

// / @brief returns the configuration of the global replication applier
globalApplier.properties = function (config) {
  if (config === undefined) {
    return GLOBAL_REPLICATION_APPLIER_CONFIGURE();
  }

  return GLOBAL_REPLICATION_APPLIER_CONFIGURE(config);
};

globalApplier.failoverEnabled = function () {
  return GLOBAL_REPLICATION_APPLIER_FAILOVER_ENABLED();
};

// / @brief performs a one-time synchronization with a remote endpoint
function sync (config) { return REPLICATION_SYNCHRONIZE(config); }

// / @brief performs a one-time synchronization with a remote endpoint
function syncGlobal (config) { return GLOBAL_REPLICATION_SYNCHRONIZE(config); }

// / @brief performs a one-time synchronization with a remote endpoint
function syncCollection (collection, config) {
  config = config || { };
  config.restrictType = 'include';
  config.restrictCollections = [ collection ];
  config.includeSystem = true;
  if (!config.hasOwnProperty('verbose')) {
    config.verbose = false;
  }

  return REPLICATION_SYNCHRONIZE(config);
}

// / @brief sets up the replication (all-in-one function for initial
// / synchronization and continuous replication)
function setup (global, config) {
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
  config.keepBarrier = true;

  var worker = global ? globalApplier : applier;
  try {
    // stop previous instance
    worker.stop();
  } catch (err) {}
  // remove existing configuration
  worker.forget();

  // run initial sync
  var result = (global ? syncGlobal : sync)(config);

  // store applier configuration
  worker.properties(config);

  worker.start(result.lastLogTick, result.barrierId);
  return worker.state();
}

function setupReplication (config) {
  return setup(false, config);
}

function setupReplicationGlobal (config) {
  return setup(true, config);
}

// / @brief returns the server's id
function serverId () {
  return internal.serverId();
}

exports.logger = logger;
exports.applier = applier;
exports.globalApplier = globalApplier;
exports.sync = sync;
exports.syncGlobal = syncGlobal;
exports.syncCollection = syncCollection;
exports.setupReplication = setupReplication;
exports.setupReplicationGlobal = setupReplicationGlobal;
exports.serverId = serverId;
exports.compareTicks = rpc.compareTicks;
