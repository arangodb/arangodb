/* global ArangoServerState */
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
var ERRORS = internal.errors;
var endpointToURL = require('@arangodb/cluster').endpointToURL;
var request;
if (ArangoServerState.role() === 'PRIMARY') {
  request = require('@arangodb/request').clusterRequest;
} else {
  request = require('@arangodb/request').request;
}

var logger = { };
var applier = { };

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the replication logger state
// //////////////////////////////////////////////////////////////////////////////

logger.state = function () {
  return internal.getStateReplicationLogger();
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the tick ranges provided by the replication logger
// //////////////////////////////////////////////////////////////////////////////

logger.tickRanges = function () {
  return internal.tickRangesReplicationLogger();
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the first tick that can be provided by the replication logger
// //////////////////////////////////////////////////////////////////////////////

logger.firstTick = function () {
  return internal.firstTickReplicationLogger();
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief starts the replication applier
// //////////////////////////////////////////////////////////////////////////////

applier.start = function (initialTick, barrierId) {
  if (initialTick === undefined) {
    return internal.startReplicationApplier();
  }

  return internal.startReplicationApplier(initialTick, barrierId);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief shuts down the replication applier
// //////////////////////////////////////////////////////////////////////////////

applier.shutdown = applier.stop = function () {
  return internal.shutdownReplicationApplier();
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the replication applier state
// //////////////////////////////////////////////////////////////////////////////

applier.state = function () {
  return internal.getStateReplicationApplier();
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief stop the applier and "forget" all configuration
// //////////////////////////////////////////////////////////////////////////////

applier.forget = function () {
  return internal.forgetStateReplicationApplier();
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief returns the configuration of the replication applier
// //////////////////////////////////////////////////////////////////////////////

applier.properties = function (config) {
  if (config === undefined) {
    return internal.configureReplicationApplier();
  }

  return internal.configureReplicationApplier(config);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief performs a one-time synchronization with a remote endpoint
// //////////////////////////////////////////////////////////////////////////////

function sync (config) {
  return internal.synchronizeReplication(config);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief performs a one-time synchronization with a remote endpoint
// //////////////////////////////////////////////////////////////////////////////

function syncCollection (collection, config) {
  config = config || { };
  config.restrictType = 'include';
  config.restrictCollections = [ collection ];
  config.includeSystem = true;
  if (!config.hasOwnProperty('verbose')) {
    config.verbose = false;
  }

  return internal.synchronizeReplication(config);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief sets up the replication (all-in-one function for initial
// / synchronization and continuous replication)
// //////////////////////////////////////////////////////////////////////////////

function setupReplication (config) {
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

  try {
    // stop previous instance
    applier.stop();
  } catch (err) {}
  // remove existing configuration
  applier.forget();

  // run initial sync
  var result = internal.synchronizeReplication(config);

  // store applier configuration
  applier.properties(config);

  applier.start(result.lastLogTick, result.barrierId);
  return applier.state();
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief returns the server's id
// //////////////////////////////////////////////////////////////////////////////

function serverId () {
  return internal.serverId();
}

exports.logger = logger;
exports.applier = applier;
exports.sync = sync;
exports.syncCollection = syncCollection;
exports.setupReplication = setupReplication;
exports.serverId = serverId;
