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
const {wait} = require("internal");
const _ = require("lodash");
const jsunity = require("../../../../common/modules/jsunity");
const {assertTrue} = jsunity.jsUnity.assertions;

const waitFor = function (checkFn, maxTries = 100) {
  let count = 0;
  let result = null;
  while (count < maxTries) {
    result = checkFn();
    if (result === true) {
      return result;
    }
    console.log(result);
    if (!(result instanceof Error)) {
      throw Error("expected error");
    }
    count += 1;
    wait(0.5);
  }
  throw result;
};

const readAgencyValueAt = function (key) {
  const response = global.ArangoAgency.get(key);
  const path = ["arango", ...key.split('/')];
  let result = response;
  for (const p of path) {
    if (result === undefined) {
      return undefined;
    }
    result = result[p];
  }
  return result;
};

const getServerRebootId = function (serverId) {
  return readAgencyValueAt(`Current/ServersKnown/${serverId}/rebootId`);
};


const getParticipantsObjectForServers = function (servers) {
  return _.reduce(servers, (a, v) => {
    a[v] = {excluded: false, forced: false};
    return a;
  }, {});
};

const createParticipantsConfig = function (generation, servers) {
  return {
    generation,
    participants: getParticipantsObjectForServers(servers),
  };
};

const createTermSpecification = function (term, servers, config, leader) {
  let spec = {term, config};
  if (leader !== undefined) {
    if (!_.includes(servers, leader)) {
      throw Error("leader is not part of the participants");
    }
    spec.leader = {serverId: leader, rebootId: getServerRebootId(leader)};
  }
  return spec;
};

const getServerHealth = function (serverId) {
  return readAgencyValueAt(`Supervision/Health/${serverId}/Status`);
};

const dbservers = (function () {
  return global.ArangoClusterInfo.getDBServers().map((x) => x.serverId);
}());


const readReplicatedLogAgency = function (database, logId) {
  let target = readAgencyValueAt(`Target/ReplicatedLogs/${database}/${logId}`);
  let plan = readAgencyValueAt(`Plan/ReplicatedLogs/${database}/${logId}`);
  let current = readAgencyValueAt(`Current/ReplicatedLogs/${database}/${logId}`);
  return {target, plan, current};
};

const replicatedLogSetPlanParticipantsConfig = function (database, logId, participantsConfig) {
  global.ArangoAgency.set(`Plan/ReplicatedLogs/${database}/${logId}/participantsConfig`, participantsConfig);
  global.ArangoAgency.increaseVersion(`Plan/Version`);
};

const replicatedLogUpdatePlanParticipantsConfigParticipants = function (database, logId, participants) {
  const oldValue = readAgencyValueAt(`Plan/ReplicatedLogs/${database}/${logId}/participantsConfig`);
  const newValue = oldValue || {generation: 0, participants: {}};
  for (const [p, v] of Object.entries(participants)) {
    if (v === null) {
      delete newValue.participants[p];
    } else {
      newValue.participants[p] = v;
    }
  }
  const gen = newValue.generation += 1;
  replicatedLogSetPlanParticipantsConfig(database, logId, newValue);
  return gen;
};

const replicatedLogSetPlanTerm = function (database, logId, term) {
  global.ArangoAgency.set(`Plan/ReplicatedLogs/${database}/${logId}/currentTerm`, term);
  global.ArangoAgency.increaseVersion(`Plan/Version`);
};

const replicatedLogSetPlan = function (database, logId, spec) {
  assertTrue(spec.targetConfig.replicationFactor <= Object.keys(spec.participantsConfig.participants).length);
  global.ArangoAgency.set(`Plan/ReplicatedLogs/${database}/${logId}`, spec);
  global.ArangoAgency.increaseVersion(`Plan/Version`);
};

const replicatedLogDeletePlan = function (database, logId) {
  global.ArangoAgency.remove(`Plan/ReplicatedLogs/${database}/${logId}`);
  global.ArangoAgency.increaseVersion(`Plan/Version`);
};

const replicatedLogIsReady = function (database, logId, term, participants, leader) {
  return function () {
    let {current} = readReplicatedLogAgency(database, logId);
    if (current === undefined) {
      return Error("current not yet defined");
    }

    for (const srv of participants) {
      if (!current.localStatus || !current.localStatus[srv]) {
        return Error(`Participant ${srv} has not yet reported to current.`);
      }
      if (current.localStatus[srv].term < term) {
        return Error(`Participant ${srv} has not yet acknowledged the current term; ` +
          `found = ${current.localStatus[srv].term}, expected = ${term}.`);
      }
    }

    if (leader !== undefined) {
      if (!current.leader) {
        return Error("Leader has not yet established its term");
      }
      if (current.leader.serverId !== leader) {
        return Error(`Wrong leader in current; found = ${current.leader.serverId}, expected = ${leader}`);
      }
      if (current.leader.term < term) {
        return Error(`Leader has not yet confirmed the term; found = ${current.leader.term}, expected = ${term}`);
      }
    }
    return true;
  };
};

const getServerProcessID = function (serverId) {
  let endpoint = global.ArangoClusterInfo.getServerEndpoint(serverId);
  // Now look for instanceInfo:
  let pos = _.findIndex(global.instanceInfo.arangods,
    x => x.endpoint === endpoint);
  return global.instanceInfo.arangods[pos].pid;
};

const stopServer = function (serverId) {
  console.log(`suspending server ${serverId}`);
  let result = require('internal').suspendExternal(getServerProcessID(serverId));
  if (!result) {
    throw Error("Failed to suspend server");
  }
};

const continueServer = function (serverId) {
  console.log(`continuing server ${serverId}`);
  let result = require('internal').continueExternal(getServerProcessID(serverId));
  if (!result) {
    throw Error("Failed to continue server");
  }
};

const allServersHealthy = function () {
  return function () {
    for (const server of dbservers) {
      const health = getServerHealth(server);
      if (health !== "GOOD") {
        return Error(`${server} is ${health}`);
      }
    }

    return true;
  };
};

const checkServerHealth = function (serverId, value) {
  return function () {
    return value === getServerHealth(serverId);
  };
};

const serverHealthy = (serverId) => checkServerHealth(serverId, "GOOD");
const serverFailed = (serverId) => checkServerHealth(serverId, "FAILED");

const continueServerWaitOk = function (serverId) {
  continueServer(serverId);
  waitFor(serverHealthy(serverId));
};

const stopServerWaitFailed = function (serverId) {
  continueServer(serverId);
  waitFor(serverFailed(serverId));
};

const nextUniqueLogId = function () {
  return parseInt(global.ArangoClusterInfo.uniqid());
};

const registerAgencyTestBegin = function (testName) {
  global.ArangoAgency.set(`Testing/${testName}/Begin`, (new Date()).toISOString());
};

const registerAgencyTestEnd = function (testName) {
  global.ArangoAgency.set(`Testing/${testName}/End`, (new Date()).toISOString());
};

exports.waitFor = waitFor;
exports.readAgencyValueAt = readAgencyValueAt;
exports.createParticipantsConfig = createParticipantsConfig;
exports.createTermSpecification = createTermSpecification;
exports.dbservers = dbservers;
exports.readReplicatedLogAgency = readReplicatedLogAgency;
exports.replicatedLogSetPlanParticipantsConfig = replicatedLogSetPlanParticipantsConfig;
exports.replicatedLogUpdatePlanParticipantsConfigParticipants = replicatedLogUpdatePlanParticipantsConfigParticipants;
exports.replicatedLogSetPlanTerm = replicatedLogSetPlanTerm;
exports.replicatedLogSetPlan = replicatedLogSetPlan;
exports.replicatedLogDeletePlan = replicatedLogDeletePlan;
exports.replicatedLogIsReady = replicatedLogIsReady;
exports.stopServer = stopServer;
exports.continueServer = continueServer;
exports.nextUniqueLogId = nextUniqueLogId;
exports.registerAgencyTestBegin = registerAgencyTestBegin;
exports.registerAgencyTestEnd = registerAgencyTestEnd;
exports.allServersHealthy = allServersHealthy;
exports.continueServerWaitOk = continueServerWaitOk;
exports.stopServerWaitFailed = stopServerWaitFailed;
