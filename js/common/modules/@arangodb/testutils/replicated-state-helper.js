/*jshint strict: true */
'use strict';
////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License")
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

const _ = require("lodash");
const LH = require("@arangodb/testutils/replicated-logs-helper");
const request = require('@arangodb/request');
const spreds = require("@arangodb/testutils/replicated-state-predicates");
const helper = require('@arangodb/test-helper');

/**
 * @param {string} database
 * @param {string} logId
 * @returns {{
 *       target: {
 *         properties: { implementation: { type: string } },
 *         participants: {},
 *         leader?: string,
 *         id: number,
 *         config: {
 *           writeConcern: number,
 *           softWriteConern: number,
 *           replicationFactor: number,
 *           waitForSync: boolean
 *         }
 *       },
 *       plan: {
 *         properties: { implementation: { type: string } },
 *         participants: Object<string, {generation: number}>,
 *         id: number,
 *         generation: number
 *       },
 *       current: {
 *         participants: Object<string, {
 *           snapshot: {
 *             timestamp: string,
 *             status: "InProgress" | "Completed" | "Failed" | "Uninitialized"
 *           }
 *         }>
 *       }
 *   }}
 */
const readReplicatedStateAgency = function (database, logId) {
  let target = LH.readAgencyValueAt(`Target/ReplicatedStates/${database}/${logId}`);
  let plan = LH.readAgencyValueAt(`Plan/ReplicatedStates/${database}/${logId}`);
  let current = LH.readAgencyValueAt(`Current/ReplicatedStates/${database}/${logId}`);
  return {target, plan, current};
};

const updateReplicatedStatePlan = function (database, logId, callback) {
  let {plan: planState} = readReplicatedStateAgency(database, logId);
  let {plan: planLog} = LH.readReplicatedLogAgency(database, logId);
  if (planState === undefined) {
    planState = {id: logId, generation: 1};
  }
  if (planLog === undefined) {
    planLog = {id: logId};
  }
  const {state, log} = callback(_.cloneDeep(planState), _.cloneDeep(planLog));
  if (!_.isEqual(planState, state)) {
    helper.agency.set(`Plan/ReplicatedStates/${database}/${logId}`, state);
  }
  if (!_.isEqual(planLog, log)) {
    helper.agency.set(`Plan/ReplicatedLogs/${database}/${logId}`, log);
  }
  helper.agency.increaseVersion(`Plan/Version`);
};

const replicatedStateDeletePlan = function (database, logId) {
  helper.agency.remove(`Plan/ReplicatedStates/${database}/${logId}`);
  helper.agency.increaseVersion(`Plan/Version`);
};

const replicatedStateDeleteTarget = function (database, logId) {
  helper.agency.remove(`Target/ReplicatedStates/${database}/${logId}`);
  helper.agency.increaseVersion(`Target/Version`);
};


const getLocalStatus = function (serverId, database, logId) {
  let url = LH.getServerUrl(serverId);
  const res = request.get(`${url}/_db/${database}/_api/replicated-state/${logId}/local-status`);
  LH.checkRequestResult(res);
  return res.json.result;
};

const updateReplicatedStateTarget = function (database, stateId, callback) {
  let {target: targetState} = readReplicatedStateAgency(database, stateId);

  const state = callback(targetState);

  helper.agency.set(`Target/ReplicatedStates/${database}/${stateId}`, state);
  helper.agency.increaseVersion(`Target/Version`);
};

const createReplicatedStateTargetWithServers = function (database, targetConfig, type, servers) {
  const stateId = LH.nextUniqueLogId();

  let participants = {};
  for (const server of servers) {
    participants[server] = {};
  }

  updateReplicatedStateTarget(database, stateId,
      function () {
        return {
          id: stateId,
          participants: participants,
          config: targetConfig,
          properties: {
            implementation: {
              type: type
            }
          }
        };
      });

  LH.waitFor(spreds.replicatedStateIsReady(database, stateId, servers));
  const leader = LH.getReplicatedLogLeaderPlan(database, stateId).leader;
  const followers = _.difference(servers, [leader]);
  const others = _.difference(LH.dbservers, servers);
  return {stateId, servers, leader, followers, others};
};

const getReplicatedStateStatus = function (database, stateId) {
  const {current} = readReplicatedStateAgency(database, stateId);
  if (current === undefined || current.supervision === undefined || current.supervision.statusReport === undefined) {
    return undefined;
  }
  return current.supervision.statusReport;
};

const createReplicatedStateTarget = function (database, targetConfig, type, replicationFactor) {
  if (replicationFactor === undefined) {
    replicationFactor = 3;
  }
  const servers = _.sampleSize(LH.dbservers, replicationFactor);
  return createReplicatedStateTargetWithServers(database, targetConfig, type, servers);
};

const getReplicatedStateLeaderTarget = function (database, logId) {
  let {target} = readReplicatedStateAgency(database, logId);
  return target.leader;
};

/**
 * Returns the value of a key from a follower.
 */
const getLocalValue = function (endpoint, db, col, key) {
  let res = request.get({
    url: `${endpoint}/_db/${db}/_api/document/${col}/${key}`,
    headers: {
      "X-Arango-Allow-Dirty-Read": true
    },
  });
  LH.checkRequestResult(res, true);
  return res.json;
};

exports.createReplicatedStateTarget = createReplicatedStateTarget;
exports.createReplicatedStateTargetWithServers = createReplicatedStateTargetWithServers;
exports.getLocalStatus = getLocalStatus;
exports.getReplicatedStateLeaderTarget = getReplicatedStateLeaderTarget;
exports.getReplicatedStateStatus = getReplicatedStateStatus;
exports.readReplicatedStateAgency = readReplicatedStateAgency;
exports.replicatedStateDeletePlan = replicatedStateDeletePlan;
exports.replicatedStateDeleteTarget = replicatedStateDeleteTarget;
exports.updateReplicatedStatePlan = updateReplicatedStatePlan;
exports.updateReplicatedStateTarget = updateReplicatedStateTarget;
exports.getLocalValue = getLocalValue;
