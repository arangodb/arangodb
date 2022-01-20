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
  let target =  LH.readAgencyValueAt(`Target/ReplicatedState/${database}/${logId}`);
  let plan =  LH.readAgencyValueAt(`Plan/ReplicatedState/${database}/${logId}`);
  let current =  LH.readAgencyValueAt(`Current/ReplicatedState/${database}/${logId}`);
  return {target, plan, current};
};

const updateReplicatedStatePlan = function (database, logId, callback) {
  let {plan} = readReplicatedStateAgency(database, logId);
  if (plan === undefined) {
    plan = {id: logId, generation: 1};
  }
  callback(plan);
  global.ArangoAgency.set(`Plan/ReplicatedState/${database}/${logId}`, plan);
  global.ArangoAgency.increaseVersion(`Plan/Version`);
};

exports.readReplicatedStateAgency = readReplicatedStateAgency;
exports.updateReplicatedStatePlan = updateReplicatedStatePlan;
