/* global ArangoServerState, GLOBAL_REPLICATION_APPLIER_START, GLOBAL_REPLICATION_APPLIER_STOP, GLOBAL_REPLICATION_APPLIER_STATE, GLOBAL_REPLICATION_APPLIER_FORGET, GLOBAL_REPLICATION_APPLIER_CONFIGURE, REPLICATION_APPLIER_START, REPLICATION_APPLIER_STOP, REPLICATION_APPLIER_STATE, REPLICATION_APPLIER_STATE_ALL, REPLICATION_APPLIER_FORGET, REPLICATION_APPLIER_CONFIGURE, GLOBAL_REPLICATION_APPLIER_FAILOVER_ENABLED */
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
const rpc = require('@arangodb/replication-common');

let logger = { };
let applier = { };
let globalApplier = { };

// / @brief return the replication logger state
logger.state = function () {
  return internal.getStateReplicationLogger();
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

exports.logger = logger;
exports.applier = applier;
exports.globalApplier = globalApplier;
exports.compareTicks = rpc.compareTicks;
