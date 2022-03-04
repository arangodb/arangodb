/*jshint strict: true */
/*global assertTrue, assertEqual*/
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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const arangodb = require("@arangodb");
const request = require("@arangodb/request");
const _ = require('lodash');
const {sleep} = require('internal');
const db = arangodb.db;
const ERRORS = arangodb.errors;
const ArangoError = arangodb.ArangoError;
const lh = require("@arangodb/testutils/replicated-logs-helper");
const sh = require("@arangodb/testutils/replicated-state-helper");
const spreds = require("@arangodb/testutils/replicated-state-predicates");

const database = "replication2_prototype_state_test_db";

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

const getServerUrl = function (serverId) {
  let endpoint = global.ArangoClusterInfo.getServerEndpoint(serverId);
  return endpoint.replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:');
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

const readReplicatedLogAgency = function (database, logId) {
  let target = readAgencyValueAt(`Target/ReplicatedLogs/${database}/${logId}`);
  let plan = readAgencyValueAt(`Plan/ReplicatedLogs/${database}/${logId}`);
  let current = readAgencyValueAt(`Current/ReplicatedLogs/${database}/${logId}`);
  return {target, plan, current};
};

const getReplicatedLogLeaderPlan = function (database, logId) {
  let {plan} = readReplicatedLogAgency(database, logId);
  if (!plan.currentTerm) {
    throw Error("no current term in plan");
  }
  if (!plan.currentTerm.leader) {
    throw Error("current term has no leader");
  }
  const leader = plan.currentTerm.leader.serverId;
  const term = plan.currentTerm.term;
  return {leader, term};
};

const updateReplicatedStateTarget = function(dbname, stateId, callback) {
  let {target: targetState} = sh.readReplicatedStateAgency(database, stateId);

  const state = callback(targetState);

  global.ArangoAgency.set(`Target/ReplicatedStates/${database}/${stateId}`, state);
  global.ArangoAgency.increaseVersion(`Target/Version`);
};

const registerAgencyTestBegin = function (testName) {
  global.ArangoAgency.set(`Testing/${testName}/Begin`, (new Date()).toISOString());
};

const registerAgencyTestEnd = function (testName) {
  global.ArangoAgency.set(`Testing/${testName}/End`, (new Date()).toISOString());
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

    sleep(1);
  }
};

const replicatedStateSuite = function () {
  const targetConfig = {
    writeConcern: 2,
    softWriteConcern: 2,
    replicationFactor: 3,
    waitForSync: false,
  };

  const {setUpAll, tearDownAll, stopServer, continueServer, resumeAll} = (function () {
    let previousDatabase, databaseExisted = true;
    let stoppedServers = {};
    return {
      setUpAll: function () {
        previousDatabase = db._name();
        if (!_.includes(db._databases(), database)) {
          db._createDatabase(database);
          databaseExisted = false;
        }
        db._useDatabase(database);
      },

      tearDownAll: function () {
        db._useDatabase(previousDatabase);
        if (!databaseExisted) {
          db._dropDatabase(database);
        }
      },
      stopServer: function (serverId) {
        if (stoppedServers[serverId] !== undefined) {
          throw Error(`{serverId} already stopped`);
        }
        lh.stopServer(serverId);
        stoppedServers[serverId] = true;
      },
      continueServer: function (serverId) {
        if (stoppedServers[serverId] === undefined) {
          throw Error(`{serverId} not stopped`);
        }
        lh.continueServer(serverId);
        delete stoppedServers[serverId];
      },
      resumeAll: function () {
        Object.keys(stoppedServers).forEach(function (key) {
          continueServer(key);
        });
      }
    };
  }());

  return {
    setUpAll, tearDownAll,
    setUp: registerAgencyTestBegin,
    tearDown: registerAgencyTestEnd,

    // Request creation of a replicated state by
    // writing a configuration to Target
    //
    // Await availability of the requested state
    testCreateReplicatedState: function() {
      const stateId = lh.nextUniqueLogId();

      const servers = _.sampleSize(lh.dbservers, 3);
      let participants = {};
      for (const server of servers) {
        participants[server] = {};
      }

      updateReplicatedStateTarget(database, stateId,
                                  function(target) {
                                    return { id: stateId,
                                             participants: participants,
                                             config: { waitForSync: true,
                                                       writeConcern: 2,
                                                       softWriteConcern: 3,
					               replicationFactor: 3},
                                             properties: {
                                               implementation: {
                                                 type: "prototype"
                                             }}};
                                  });

      lh.waitFor(spreds.replicatedStateIsReady(database, stateId, servers));
      waitForReplicatedLogAvailable(stateId);

      let {leader} = getReplicatedLogLeaderPlan(database, stateId);
      let url = getServerUrl(leader);

      let coord = lh.coordinators[0];
      let coordUrl = getServerUrl(coord);

      let result = request.post({url: `${url}/_db/${database}/_api/prototype-state/${stateId}/insert`, body: {foo : "bar"}, json: true });
      checkRequestResult(result);
      require('internal').print("################################################");
      require('internal').print(result.json);


      result = request.post({url: `${url}/_db/${database}/_api/prototype-state/${stateId}/insert`, body: {foo1 : "bar1", foo2 : "bar2"}, json: true });
      checkRequestResult(result);
      require('internal').print("################################################");
      require('internal').print(result.json);

      result = request.get({url: `${url}/_db/${database}/_api/prototype-state/${stateId}/entry/foo`});
      checkRequestResult(result);
      require('internal').print("################################################");
      require('internal').print(result.json);

      result = request.get({url: `${url}/_db/${database}/_api/prototype-state/${stateId}/multi-get?key[]=foo1&key[]=foo2`});
      checkRequestResult(result);
      require('internal').print("################################################");
      require('internal').print(result.json);

      result = request.delete({url: `${url}/_db/${database}/_api/prototype-state/${stateId}/entry/foo`});
      checkRequestResult(result);
      require('internal').print("################################################");
      require('internal').print(result.json);

      result = request.get({url: `${url}/_db/${database}/_api/prototype-state/${stateId}/entry/foo`});
      require('internal').print("################################################");
      require('internal').print(result.json);

      result = request.delete({url: `${url}/_db/${database}/_api/prototype-state/${stateId}/multi-remove`, body: ["foo1", "foo2"], json: true });
      checkRequestResult(result);
      require('internal').print("################################################");
      require('internal').print(result.json);

      result = request.get({url: `${url}/_db/${database}/_api/prototype-state/${stateId}/entry/foo1`});
      require('internal').print("################################################");
      require('internal').print(result.json);

      result = request.post({url: `${coordUrl}/_db/${database}/_api/prototype-state/${stateId}/insert`, body: {foo100 : "bar100"}, json: true });
      checkRequestResult(result);
      require('internal').print("################################################");
      require('internal').print(result.json);

      result = request.get({url: `${url}/_db/${database}/_api/prototype-state/${stateId}/entry/foo100`});
      require('internal').print("################################################");
      require('internal').print(result.json);

      result = request.post({url: `${coordUrl}/_db/${database}/_api/prototype-state/${stateId}/insert`, body: {foo200 : "bar200", foo300 : "bar300"}, json: true });
      checkRequestResult(result);
      require('internal').print("################################################");
      require('internal').print(result.json);

      result = request.get({url: `${coordUrl}/_db/${database}/_api/prototype-state/${stateId}/entry/foo300`});
      require('internal').print("################################################");
      require('internal').print(result.json);

      result = request.get({url: `${coordUrl}/_db/${database}/_api/prototype-state/${stateId}/multi-get?key[]=foo300&key[]=abcd&key[]=foo200`});
      require('internal').print("################################################");
      require('internal').print(result.json);
    },
  };
};

jsunity.run(replicatedStateSuite);
return jsunity.done();
