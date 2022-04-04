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
  let target =  LH.readAgencyValueAt(`Target/ReplicatedStates/${database}/${logId}`);
  let plan =  LH.readAgencyValueAt(`Plan/ReplicatedStates/${database}/${logId}`);
  let current =  LH.readAgencyValueAt(`Current/ReplicatedStates/${database}/${logId}`);
  return {target, plan, current};
};

const updateReplicatedStatePlan = function (database, logId, callback) {
  let {plan: planState} = readReplicatedStateAgency(database, logId);
  let {plan: planLog} = LH.readReplicatedLogAgency(database, logId);
  if (planState === undefined ) {
    planState = {id: logId, generation: 1};
  }
  if (planLog === undefined) {
    planLog = {id: logId};
  }
  const {state, log} = callback(planState, planLog);
  global.ArangoAgency.set(`Plan/ReplicatedStates/${database}/${logId}`, state);
  global.ArangoAgency.set(`Plan/ReplicatedLogs/${database}/${logId}`, log);
  global.ArangoAgency.increaseVersion(`Plan/Version`);
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

  global.ArangoAgency.set(`Target/ReplicatedStates/${database}/${stateId}`, state);
  global.ArangoAgency.increaseVersion(`Target/Version`);
};

const createReplicatedStateTarget = function (database, targetConfig, type) {
  const stateId = LH.nextUniqueLogId();

  const servers = _.sampleSize(LH.dbservers, targetConfig.replicationFactor);
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
  return {stateId, servers, leader, followers};
};

exports.readReplicatedStateAgency = readReplicatedStateAgency;
exports.updateReplicatedStatePlan = updateReplicatedStatePlan;
exports.getLocalStatus = getLocalStatus;
exports.createReplicatedStateTarget = createReplicatedStateTarget;
exports.updateReplicatedStateTarget = updateReplicatedStateTarget;
