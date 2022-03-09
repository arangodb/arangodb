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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////
const jsunity = require('jsunity');
const arangodb = require("@arangodb");
const _ = require('lodash');
const db = arangodb.db;
const lh = require("@arangodb/testutils/replicated-logs-helper");
const sh = require("@arangodb/testutils/replicated-state-helper");
const spreds = require("@arangodb/testutils/replicated-state-predicates");
const request = require('@arangodb/request');

const database = "replication2_replicated_state_http_api_db";

const replicatedStateSuite = function () {
  const targetConfig = {
    writeConcern: 2,
    softWriteConcern: 2,
    replicationFactor: 3,
    waitForSync: false,
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

  return {
    setUpAll, tearDownAll,
    setUp: lh.registerAgencyTestBegin,
    tearDown: lh.registerAgencyTestEnd,

    testGetLocalStatus: function () {
      const {stateId, followers, leader} = sh.createReplicatedStateTarget(database, targetConfig, "black-hole");

      {
        const status = sh.getLocalStatus(leader, database, stateId);
        assertEqual(status.role, "leader");
        assertEqual(status.manager.managerState, "ServiceAvailable");

        assertEqual(status.snapshot.status, "Completed");
        assertTrue(status.snapshot.timestamp !== undefined);
        assertEqual(status.generation, 1);

      }

      for (const server of followers) {
        const status = sh.getLocalStatus(server, database, stateId);
        assertEqual(status.role, "follower");
        assertEqual(status.manager.managerState, "NothingToApply");
        assertEqual(status.snapshot.status, "Completed");
        assertTrue(status.snapshot.timestamp !== undefined);
        assertEqual(status.generation, 1);
      }
    },
  };
};

jsunity.run(replicatedStateSuite);
return jsunity.done();
