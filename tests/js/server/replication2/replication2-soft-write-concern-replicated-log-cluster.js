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

const jsunity = require('jsunity');
const {assertEqual} = jsunity.jsUnity.assertions;
const arangodb = require("@arangodb");
const _ = require('lodash');
const db = arangodb.db;
const helper = require("@arangodb/testutils/replicated-logs-helper");
const lpreds = require("@arangodb/testutils/replicated-logs-predicates");
const {
  waitFor,
  registerAgencyTestBegin, registerAgencyTestEnd,
  waitForReplicatedLogAvailable,
} = helper;

const database = "replication2_soft_write_concern_test_db";

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

  const createReplicatedLogAndWaitForLeader = function (database) {
    return helper.createReplicatedLog(database, targetConfig);
  };

  return {
    setUpAll, tearDownAll,
    setUp: registerAgencyTestBegin,
    tearDown: function (test) {
      resumeAll();
      waitFor(lpreds.allServersHealthy());
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
