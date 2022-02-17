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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
const jsunity = require('jsunity');
const arangodb = require("@arangodb");
const _ = require('lodash');
const {sleep} = require('internal');
const db = arangodb.db;
const ERRORS = arangodb.errors;
const helper = require("@arangodb/testutils/replicated-logs-helper");

const {
  waitFor,
  readReplicatedLogAgency,
  replicatedLogSetTarget,
  dbservers, getParticipantsObjectForServers,
  nextUniqueLogId, allServersHealthy,
  registerAgencyTestBegin, registerAgencyTestEnd,
  replicatedLogLeaderEstablished,
} = helper;

const database = "replication2_soft_write_concern_test_db";

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

const replicatedLogSuite = function () {

  const targetConfig = {
    writeConcern: 2,
    softWriteConcern: 3,
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
          throw Error(`${serverId} already stopped`);
        }
        helper.stopServer(serverId);
        stoppedServers[serverId] = true;
      },
      continueServer: function (serverId) {
        if (stoppedServers[serverId] === undefined) {
          throw Error(`${serverId} not stopped`);
        }
        helper.continueServer(serverId);
        delete stoppedServers[serverId];
      },
      resumeAll: function () {
        Object.keys(stoppedServers).forEach(function (key) {
          continueServer(key);
        });
      }
    };
  }());

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

  const createReplicatedLogAndWaitForLeader = function (database) {
    const logId = nextUniqueLogId();
    const servers = _.sampleSize(dbservers, targetConfig.replicationFactor);
    replicatedLogSetTarget(database, logId, {
      id: logId,
      config: targetConfig,
      participants: getParticipantsObjectForServers(servers),
      supervision: {maxActionsTraceLength: 20},
    });

    waitFor(replicatedLogLeaderEstablished(database, logId, undefined, servers));

    const {leader, term} = getReplicatedLogLeaderPlan(database, logId);
    const followers = _.difference(servers, [leader]);
    return {logId, servers, leader, term, followers};
  };

  return {
    setUpAll, tearDownAll,
    setUp: registerAgencyTestBegin,
    tearDown: function (test) {
      resumeAll();
      waitFor(allServersHealthy());
      registerAgencyTestEnd(test);
    },

    testParticipantFailedOnInsert: function () {
      const {logId, followers} = createReplicatedLogAndWaitForLeader(database);
      waitForReplicatedLogAvailable(logId);

      let log = db._replicatedLog(logId);
      let quorum = log.insert({foo: "bar"});
      assertEqual(quorum.result.quorum.quorum.length, 3);

      // now stop one server
      stopServer(followers[0]);

      quorum = log.insert({foo: "bar"});
      assertEqual(quorum.result.quorum.quorum.length, 2);

      continueServer(followers[0]);

      waitFor(function () {
        quorum = log.insert({foo: "bar"});
        if (quorum.result.quorum.quorum.length !== 3) {
          return Error(`quorum size not reached, found ${JSON.stringify(quorum.result)}`);
        }
        return true;
      });
    },
  };
};

jsunity.run(replicatedLogSuite);
return jsunity.done();
