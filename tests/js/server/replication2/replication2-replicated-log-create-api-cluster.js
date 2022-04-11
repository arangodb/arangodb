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
const {db} = arangodb;
const lh = require("@arangodb/testutils/replicated-logs-helper");

const database = "replication2_entry_test_db_create";

const replicatedLogCreateSuite = function () {
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
      }
    };
  }());

  return {
    setUpAll, tearDownAll,
    setUp: lh.registerAgencyTestBegin,
    tearDown: lh.registerAgencyTestEnd,

    testCreateWithExplicitId: function () {
      const logId = lh.nextUniqueLogId();
      const log = db._createReplicatedLog({id: logId, config: targetConfig});
      assertEqual(logId, log.id());
    },

    testCreateWithExplicitLeader: function () {
      const leader = _.sample(lh.dbservers);
      const log = db._createReplicatedLog({leader, config: targetConfig});
      const status = log.status();
      assertEqual(status.leaderId, leader);
    },

    testCreateWithExplicitServers: function () {
      const servers = _.sampleSize(lh.dbservers, 3);
      const log = db._createReplicatedLog({servers, config: targetConfig});
      const status = log.status();
      assertEqual(Object.keys(status.participants).sort(), servers.sort());
    },
  };
};

jsunity.run(replicatedLogCreateSuite);
return jsunity.done();
