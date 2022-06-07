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

const internal = require("internal");
const {wait} = internal;
const _ = require("lodash");
const request = require('@arangodb/request');
const arangodb = require('@arangodb');
const ArangoError = arangodb.ArangoError;
const ERRORS = arangodb.errors;
const db = arangodb.db;
const lpreds = require("@arangodb/testutils/replicated-logs-predicates");

const waitFor = function (checkFn, maxTries = 240) {
  let count = 0;
  let result = null;
  while (count < maxTries) {
    result = checkFn();
    if (result === true || result === undefined) {
      return result;
    }
    if (!(result instanceof Error)) {
      throw Error("expected error");
    }
    count += 1;
    if (count % 10 === 0) {
      console.log(result);
    }
    wait(0.5); // 240 * .5s = 2 minutes
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
    a[v] = {allowedInQuorum: true, allowedAsLeader: true, forced: false};
    return a;
  }, {});
};

const createParticipantsConfig = function (generation, config, servers) {
  return {
    generation,
    config,
    participants: getParticipantsObjectForServers(servers),
  };
};

const createTermSpecification = function (term, servers, leader) {
  let spec = {term};
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
const coordinators = (function () {
  return global.ArangoClusterInfo.getCoordinators();
}());


/**
 * @param {string} database
 * @param {string} logId
 * @returns {{
 *       target: {
 *         id: number,
 *         leader?: string,
 *         participants: Object<string, {
 *           forced: boolean,
 *           allowedInQuorum: boolean,
 *           allowedAsLeader: boolean
 *         }>,
 *         config: {
 *           writeConcern: number,
 *           softWriteConern: number,
 *           replicationFactor: number,
 *           waitForSync: boolean
 *         },
 *         properties: {}
 *       },
 *       plan: {
 *         id: number,
 *         participantsConfig: Object,
 *         currentTerm?: Object
 *       },
 *       current: {
 *         localState: Object,
 *         supervision?: Object,
 *         leader?: Object
 *       }
 *   }}
 */
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

const waitForReplicatedLogAvailable = function (id) {
  while (true) {
    try {
      let status = db._replicatedLog(id).status();
      const leaderId = status.leaderId;
      if (leaderId !== undefined && status.participants !== undefined &&
          status.participants[leaderId].connection.errorCode === 0 && status.participants[leaderId].response.role === "leader") {
        break;
      }
      console.info("replicated log not yet available");
    } catch (err) {
      const errors = [
        ERRORS.ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED.code,
        ERRORS.ERROR_REPLICATION_REPLICATED_LOG_NOT_FOUND.code
      ];
      if (errors.indexOf(err.errorNum) === -1) {
        throw err;
      }
    }

    internal.sleep(1);
  }
};


const getServerProcessID = function (serverId) {
  let endpoint = global.ArangoClusterInfo.getServerEndpoint(serverId);
  // Now look for instanceInfo:
  let pos = _.findIndex(global.instanceInfo.arangods,
      x => x.endpoint === endpoint);
  return global.instanceInfo.arangods[pos].pid;
};

const stopServerImpl = function (serverId) {
  console.log(`suspending server ${serverId}`);
  let result = require('internal').suspendExternal(getServerProcessID(serverId));
  if (!result) {
    throw Error("Failed to suspend server");
  }
};

const continueServerImpl = function (serverId) {
  console.log(`continuing server ${serverId}`);
  let result = require('internal').continueExternal(getServerProcessID(serverId));
  if (!result) {
    throw Error("Failed to continue server");
  }
};

const continueServerWaitOk = function (serverId) {
  continueServerImpl(serverId);
  waitFor(lpreds.serverHealthy(serverId));
};

const stopServerWaitFailed = function (serverId) {
  stopServerImpl(serverId);
  waitFor(lpreds.serverFailed(serverId));
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

const getReplicatedLogLeaderPlan = function (database, logId, nothrow = false) {
  let {plan} = readReplicatedLogAgency(database, logId);
  if (!plan.currentTerm) {
    throw Error("no current term in plan");
        let error = Error("no current term in plan");
    if (nothrow) {
      return error;
    }
    throw error;
  }
  if (!plan.currentTerm.leader) {
    let error = Error("current term has no leader");
    if (nothrow) {
      return error;
    }
    throw error;
  }
  const leader = plan.currentTerm.leader.serverId;
  const term = plan.currentTerm.term;
  return {leader, term};
};

const getReplicatedLogLeaderTarget = function (database, logId) {
  let {target} = readReplicatedLogAgency(database, logId);
  return target.leader;
};

const createReplicatedLogPlanOnly = function (database, targetConfig, replicationFactor) {
  const logId = nextUniqueLogId();
  const servers = _.sampleSize(dbservers, replicationFactor);
  const leader = servers[0];
  const term = 1;
  const generation = 1;
  replicatedLogSetPlan(database, logId, {
    id: logId,
    currentTerm: createTermSpecification(term, servers, leader),
    participantsConfig: createParticipantsConfig(generation, targetConfig, servers),
  });

  // wait for all servers to have reported in current
  waitFor(lpreds.replicatedLogIsReady(database, logId, term, servers, leader));
  const followers = _.difference(servers, [leader]);
  const remaining = _.difference(dbservers, servers);
  return {logId, servers, leader, term, followers, remaining};
};

const createReplicatedLog = function (database, targetConfig, replicationFactor) {
  const logId = nextUniqueLogId();
  if (replicationFactor === undefined) {
    replicationFactor = 3;
  }
  const servers = _.sampleSize(dbservers, replicationFactor);
  replicatedLogSetTarget(database, logId, {
    id: logId,
    config: targetConfig,
    participants: getParticipantsObjectForServers(servers),
    supervision: {maxActionsTraceLength: 20},
  });

  waitFor(lpreds.replicatedLogLeaderEstablished(database, logId, undefined, servers));

  const {leader, term} = getReplicatedLogLeaderPlan(database, logId);
  const followers = _.difference(servers, [leader]);
  return {logId, servers, leader, term, followers};
};


const testHelperFunctions = function (database) {
  let previousDatabase, databaseExisted = true;
  let stoppedServers = {};

  const stopServer = function (serverId) {
    if (stoppedServers[serverId] !== undefined) {
      throw new Error(`{serverId} already stopped`);
    }
    stopServerImpl(serverId);
    stoppedServers[serverId] = true;
  };

  const stopServerWait = function (serverId) {
    stopServer(serverId);
    waitFor(lpreds.serverFailed(serverId));
  };

  const continueServer = function (serverId) {
    if (stoppedServers[serverId] === undefined) {
      throw new Error(`{serverId} not stopped`);
    }
    continueServerImpl(serverId);
    delete stoppedServers[serverId];
  };

  const continueServerWait = function (serverId) {
    continueServer(serverId);
    waitFor(lpreds.serverHealthy(serverId));
  };

  const resumeAll = function () {
    Object.keys(stoppedServers).forEach(function (key) {
      continueServerWait(key);
    });
    stoppedServers = {};
  };

  const createTestDatabase = function () {
    previousDatabase = db._name();
    if (!_.includes(db._databases(), database)) {
      db._createDatabase(database);
      databaseExisted = false;
    }
    db._useDatabase(database);
  };

  const resetPreviousDatabase = function () {
    db._useDatabase(previousDatabase);
    if (!databaseExisted) {
      db._dropDatabase(database);
    }
  };

  const setUpAll = function () {
    createTestDatabase();
  };

  const tearDownAll = function () {
    resumeAll();
    resetPreviousDatabase();
  };

  const setUpAnd = function (cb) {
    return function (testName) {
      registerAgencyTestBegin(testName);
      cb(testName);
    };
  };
  const setUp = setUpAnd(() => null);

  const tearDownAnd = function (cb) {
    return function (testName) {
      registerAgencyTestEnd(testName);
      resumeAll();
      cb(testName);
    };
  };
  const tearDown = tearDownAnd(() => null);

  return {
    stopServer,
    stopServerWait,
    continueServer,
    continueServerWait,
    resumeAll,
    setUpAll,
    tearDownAll,
    setUp,
    setUpAnd,
    tearDown,
    tearDownAnd
  };
};

const getSupervisionActionTypes = function (database, logId) {
  const {current} = readReplicatedLogAgency(database, logId);
  // filter out all empty actions
  const actions = _.filter(current.actions, (a) => a.desc.type !== "EmptyAction");
  return _.map(actions, (a) => a.desc.type);
};

const getLastNSupervisionActionsType = function (database, logId, n) {
  const actions = getSupervisionActionTypes(database, logId);
  return _.takeRight(actions, n);
};

const countActionsByType = function (actions) {
  return _.reduce(actions, function (acc, type) {
    acc[type] = 1 + (acc[type] || 0);
    return acc;
  }, {});
};

const updateReplicatedLogTarget = function(database, id, callback) {
  const {target: oldTarget} = readReplicatedLogAgency(database, id);
  let result = callback(oldTarget);
  if (result === undefined) {
    result = oldTarget;
  }
  replicatedLogSetTarget(database, id, result);
};

exports.checkRequestResult = checkRequestResult;
exports.continueServer = continueServerImpl;
exports.continueServerWaitOk = continueServerWaitOk;
exports.coordinators = coordinators;
exports.countActionsByType = countActionsByType;
exports.createParticipantsConfig = createParticipantsConfig;
exports.createReplicatedLog = createReplicatedLog;
exports.createReplicatedLogPlanOnly = createReplicatedLogPlanOnly;
exports.createTermSpecification = createTermSpecification;
exports.dbservers = dbservers;
exports.getLastNSupervisionActionsType = getLastNSupervisionActionsType;
exports.getLocalStatus = getLocalStatus;
exports.getParticipantsObjectForServers = getParticipantsObjectForServers;
exports.getReplicatedLogLeaderPlan = getReplicatedLogLeaderPlan;
exports.getReplicatedLogLeaderTarget = getReplicatedLogLeaderTarget;
exports.getServerHealth = getServerHealth;
exports.getServerRebootId = getServerRebootId;
exports.getServerUrl = getServerUrl;
exports.getSupervisionActionTypes = getSupervisionActionTypes;
exports.nextUniqueLogId = nextUniqueLogId;
exports.readAgencyValueAt = readAgencyValueAt;
exports.readReplicatedLogAgency = readReplicatedLogAgency;
exports.registerAgencyTestBegin = registerAgencyTestBegin;
exports.registerAgencyTestEnd = registerAgencyTestEnd;
exports.replicatedLogDeletePlan = replicatedLogDeletePlan;
exports.replicatedLogDeleteTarget = replicatedLogDeleteTarget;
exports.replicatedLogSetPlan = replicatedLogSetPlan;
exports.replicatedLogSetPlanParticipantsConfig = replicatedLogSetPlanParticipantsConfig;
exports.replicatedLogSetPlanTerm = replicatedLogSetPlanTerm;
exports.replicatedLogSetTarget = replicatedLogSetTarget;
exports.replicatedLogUpdatePlanParticipantsConfigParticipants = replicatedLogUpdatePlanParticipantsConfigParticipants;
exports.replicatedLogUpdateTargetParticipants = replicatedLogUpdateTargetParticipants;
exports.stopServer = stopServerImpl;
exports.stopServerWaitFailed = stopServerWaitFailed;
exports.testHelperFunctions = testHelperFunctions;
exports.updateReplicatedLogTarget = updateReplicatedLogTarget;
exports.waitFor = waitFor;
exports.waitForReplicatedLogAvailable = waitForReplicatedLogAvailable;
