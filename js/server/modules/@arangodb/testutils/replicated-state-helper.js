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
  callback(planState, planLog);
  global.ArangoAgency.set(`Plan/ReplicatedStates/${database}/${logId}`, planState);
  global.ArangoAgency.set(`Plan/ReplicatedLogs/${database}/${logId}`, planLog);
  global.ArangoAgency.increaseVersion(`Plan/Version`);
};

exports.readReplicatedStateAgency = readReplicatedStateAgency;
exports.updateReplicatedStatePlan = updateReplicatedStatePlan;
