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
const request = require('@arangodb/request');
const arangodb = require('@arangodb');
const ArangoError = arangodb.ArangoError;

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

const replicatedLogSetTargetParticipantsConfig = function (database, logId, participantsConfig) {
  global.ArangoAgency.set(`Target/ReplicatedLogs/${database}/${logId}/participants`, participantsConfig);
  global.ArangoAgency.increaseVersion(`Target/Version`);
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

const replicatedLogUpdateTargetParticipants = function (database, logId, participants) {
  const oldValue = readAgencyValueAt(`Target/ReplicatedLogs/${database}/${logId}/participants`);
  const newValue = oldValue || {};
  for (const [p, v] of Object.entries(participants)) {
    if (v === null) {
      delete newValue[p];
    } else {
      newValue[p] = v;
    }
  }
  replicatedLogSetTargetParticipantsConfig(database, logId, newValue);
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

const replicatedLogSetTarget = function (database, logId, spec) {
  global.ArangoAgency.set(`Target/ReplicatedLogs/${database}/${logId}`, spec);
  global.ArangoAgency.increaseVersion(`Target/Version`);
};

const replicatedLogDeletePlan = function (database, logId) {
  global.ArangoAgency.remove(`Plan/ReplicatedLogs/${database}/${logId}`);
  global.ArangoAgency.increaseVersion(`Plan/Version`);
};

const replicatedLogDeleteTarget = function (database, logId) {
  global.ArangoAgency.remove(`Target/ReplicatedLogs/${database}/${logId}`);
  global.ArangoAgency.increaseVersion(`Target/Version`);
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
      if (!current.leader.leadershipEstablished) {
        return Error("Leader has not yet established its leadership");
      }
    }
    return true;
  };
};

const replicatedLogLeaderEstablished = function (database, logId, term, participants) {
  return function () {
    let {current} = readReplicatedLogAgency(database, logId);
    if (current === undefined) {
      return Error("current not yet defined");
    }

    for (const srv of participants) {
      if (!current.localStatus || !current.localStatus[srv]) {
        return Error(`Participant ${srv} has not yet reported to current.`);
      }
      if (term !== undefined && current.localStatus[srv].term < term) {
        return Error(`Participant ${srv} has not yet acknowledged the current term; ` +
            `found = ${current.localStatus[srv].term}, expected = ${term}.`);
      }
    }

    if (!current.leader) {
      return Error("Leader has not yet established its term");
    }
    if (!current.leader.leadershipEstablished) {
      return Error("Leader has not yet established its leadership");
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

const getServerUrl = function (serverId) {
  let endpoint = global.ArangoClusterInfo.getServerEndpoint(serverId);
  return endpoint.replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:');
};

const checkRequestResult = function (requestResult) {
  if (requestResult === undefined) {
    throw new ArangoError({
      'error': true,
      'code': 500,
      'errorNum': arangodb.ERROR_INTERNAL,
      'errorMessage': 'Unknown error. Request result is empty'
    });
  }

  if (requestResult.hasOwnProperty('error')) {
    if (requestResult.error) {
      if (requestResult.errorNum === arangodb.ERROR_TYPE_ERROR) {
        throw new TypeError(requestResult.errorMessage);
      }

      const error = new ArangoError(requestResult);
      error.message = requestResult.message;
      throw error;
    }

    // remove the property from the original object
    delete requestResult.error;
  }

  if (requestResult.json.error) {
    throw new ArangoError({
      'error': true,
      'code': requestResult.json.code,
      'errorNum': arangodb.ERROR_INTERNAL,
      'errorMessage': 'Error during request'
    });
  }

  return requestResult;
};

const getLocalStatus = function (database, logId, serverId) {
  let url = getServerUrl(serverId);
  const res = request.get(`${url}/_db/${database}/_api/log/${logId}/local-status`);
  checkRequestResult(res);
  return res.json.result;
};


const replicatedLogParticipantsFlag = function (database, logId, flags, generation = undefined) {
  return function () {
    let {current} = readReplicatedLogAgency(database, logId);
    if (current === undefined) {
      return Error("current not yet defined");
    }
    if (!current.leader) {
      return Error("Leader has not yet established its term");
    }
    if (!current.leader.committedParticipantsConfig) {
      return Error("Leader has not yet committed any participants config");
    }
    if (generation !== undefined) {
      if (current.leader.committedParticipantsConfig.generation < generation) {
        return Error("Leader has not yet acked new generation; "
            + `found ${current.leader.committedParticipantsConfig.generation}, expected = ${generation}`);
      }
    }

    const participants = current.leader.committedParticipantsConfig.participants;
    for (const [p, v] of Object.entries(flags)) {
      if (v === null) {
        if (participants[p] !== undefined) {
          return Error(`Entry for server ${p} still present in participants flags`);
        }
      } else {
        if (!_.isEqual(v, participants[p])) {
          return Error(`Flags for participant ${p} are not as expected: ${JSON.stringify(v)} vs. ${JSON.stringify(participants[p])}`);
        }
      }
    }

    return true;
  };
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
exports.replicatedLogSetTarget = replicatedLogSetTarget;
exports.getParticipantsObjectForServers = getParticipantsObjectForServers;
exports.allServersHealthy = allServersHealthy;
exports.continueServerWaitOk = continueServerWaitOk;
exports.stopServerWaitFailed = stopServerWaitFailed;
exports.replicatedLogDeleteTarget = replicatedLogDeleteTarget;
exports.getLocalStatus = getLocalStatus;
exports.replicatedLogUpdateTargetParticipants = replicatedLogUpdateTargetParticipants;
exports.replicatedLogLeaderEstablished = replicatedLogLeaderEstablished;
exports.replicatedLogParticipantsFlag = replicatedLogParticipantsFlag;
