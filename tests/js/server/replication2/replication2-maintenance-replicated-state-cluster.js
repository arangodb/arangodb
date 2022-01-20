/*jshint strict: true */
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
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
const db = arangodb.db;
const _ = require('lodash');
const LH = require("@arangodb/testutils/replicated-logs-helper");
const SH = require("@arangodb/testutils/replicated-state-helper");
const spreds = require("@arangodb/testutils/replicated-state-predicates");

const database = "ReplicatedStateMaintenanceTestDB";

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

const replicatedStateSuite = function () {
  return {
    setUpAll, tearDownAll,

    testCreateReplicatedState: function () {
      const logId = LH.nextUniqueLogId();
      const servers = _.sampleSize(LH.dbservers, 3);
      SH.updateReplicatedStatePlan(database, logId, function (plan) {
        plan.participants = {};
        for (const server of servers) {
          plan.participants[server] = {
            generation: 1,
          };
        }
      });

      LH.waitFor(spreds.replicatedStateIsReady(database, logId, servers));
    },
  };
};

jsunity.run(replicatedStateSuite);
return jsunity.done();
