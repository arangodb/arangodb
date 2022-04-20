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
const lpreds = require("@arangodb/testutils/replicated-logs-predicates");
const request = require('@arangodb/request');
const {waitFor} = require("@arangodb/testutils/replicated-logs-helper");

const database = "replication2_replicated_log_http_api_db";

const logFunctions = {
  head: function (server, database, logId, limit) {
    let url = `${lh.getServerUrl(server)}/_db/${database}/_api/log/${logId}/head`;
    if (limit !== undefined) {
      url += `?limit=${limit}`;
    }
    return request.get(url).json;
  },

  tail: function (server, database, logId, limit) {
    let url = `${lh.getServerUrl(server)}/_db/${database}/_api/log/${logId}/tail`;
    if (limit !== undefined) {
      url += `?limit=${limit}`;
    }
    return request.get(url).json;
  },

  slice: function (server, database, logId, start, stop) {
    let url = `${lh.getServerUrl(server)}/_db/${database}/_api/log/${logId}/slice?start=${start}&stop=${stop}`;
    return request.get(url).json;
  },

  at: function (server, database, logId, index) {
    let url = `${lh.getServerUrl(server)}/_db/${database}/_api/log/${logId}/entry/${index}`;
    return request.get(url).json;
  },
};

const {setUpAll, tearDownAll} = (function () {
  let previousDatabase, databaseExisted = true;
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
  };
}());

const readFollowerLogs = function () {
  const targetConfig = {
    writeConcern: 3,
    softWriteConcern: 3,
    replicationFactor: 3,
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
    setUpAll, tearDownAll,
    setUp: lh.registerAgencyTestBegin,
    tearDown: lh.registerAgencyTestEnd,

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

jsunity.run(readFollowerLogs);
return jsunity.done();
