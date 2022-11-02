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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const {assertEqual, assertTrue, assertUndefined} = jsunity.jsUnity.assertions;
const internal = require("internal");
const arangodb = require("@arangodb");
const ERRORS = arangodb.errors;
const _ = require('lodash');
const db = arangodb.db;
const lh = require("@arangodb/testutils/replicated-logs-helper");

const database = "replication2_replicated_log_http_api_db";

const logFunctions = require("@arangodb/testutils/replicated-logs-http-helper");

const {setUpAll, tearDownAll, setUp, tearDown} = lh.testHelperFunctions(database);

const readFollowerLogsSuite = function () {
  const targetConfig = {
    writeConcern: 3,
    softWriteConcern: 3,
    waitForSync: false,
  };

  const setupReplicatedLog = function () {
    const log = db._createReplicatedLog({config: targetConfig});
    const {plan} = lh.readReplicatedLogAgency(database, log.id());
    const leader = plan.currentTerm.leader.serverId;
    const servers = Object.keys(plan.participantsConfig.participants);
    const followers = _.without(servers, leader);

    // write some entries
    for (let i = 2; i < 100; i++) {
      const result = log.insert({value: i});
      assertEqual(result.index, i);
    }

    return {log, leader, servers, followers};
  };

  return {
    setUpAll, tearDownAll, setUp, tearDown,

    testFollowerHead: function () {
      const {log, followers} = setupReplicatedLog();
      const response = logFunctions.head(followers[0], database, log.id(), 10);
      assertEqual(response.error, false);
      assertEqual(response.code, 200);

      assertEqual(response.result.length, 10);
      for (let i = 2; i < 10; i++) {
        const entry = response.result[i - 1];
        assertEqual(entry.logIndex, i);
        assertEqual(entry.payload.value, i);
      }
    },

    testFollowerTail: function () {
      const {log, followers} = setupReplicatedLog();
      const response = logFunctions.tail(followers[0], database, log.id(), 10);
      assertEqual(response.error, false);
      assertEqual(response.code, 200);

      assertEqual(response.result.length, 10);
      for (let i = 90; i < 100; i++) {
        const entry = response.result[i - 90];
        assertEqual(entry.logIndex, i);
        assertEqual(entry.payload.value, i);
      }
    },

    testFollowerEntry: function () {
      const {log, followers} = setupReplicatedLog();
      const idx = 10;
      const response = logFunctions.at(followers[0], database, log.id(), idx);
      assertEqual(response.error, false);
      assertEqual(response.code, 200);

      assertEqual(response.result.payload.value, idx);
      assertEqual(response.result.logIndex, idx);
    },

    testFollowerSlice: function () {
      const [start, stop] = [50, 60];
      const {log, followers} = setupReplicatedLog();
      const response = logFunctions.slice(followers[0], database, log.id(), start, stop);
      assertEqual(response.error, false);
      assertEqual(response.code, 200);

      assertEqual(response.result.length, stop - start);
      for (let i = start; i < stop; i++) {
        const entry = response.result[i - start];
        assertEqual(entry.logIndex, i);
        assertEqual(entry.payload.value, i);
      }
    },
  };
};

const logInfoSuite = function () {
  const targetConfig = {
    writeConcern: 3,
    softWriteConcern: 3,
    waitForSync: false,
  };

  return {
    setUpAll, tearDownAll, setUp, tearDown,

    testApiLog: function () {
      const log1 = lh.createReplicatedLog(database, targetConfig);
      const log2 = lh.createReplicatedLog(database, targetConfig);

      const leaderRes = logFunctions.listLogs(log1.leader, database);
      assertTrue(log1.logId in leaderRes.result);
      assertEqual(leaderRes.result[log1.logId].role, "leader");

      const follower = log1.followers[0];
      const followerRes = logFunctions.listLogs(follower, database);
      assertEqual(followerRes.result[log1.logId].role, "follower");

      const coord = lh.coordinators[0];
      const coordRes = logFunctions.listLogs(coord, database);
      assertTrue(_.isEqual(_.sortBy(coordRes.result[log1.logId]), _.sortBy(log1.servers)));
      assertTrue(_.isEqual(_.sortBy(coordRes.result[log2.logId]), _.sortBy(log2.servers)));
      assertEqual(log1.leader, coordRes.result[log1.logId][0]);
      assertEqual(log2.leader, coordRes.result[log2.logId][0]);
    },
  };
};

jsunity.run(readFollowerLogsSuite);
jsunity.run(logInfoSuite);
return jsunity.done();
