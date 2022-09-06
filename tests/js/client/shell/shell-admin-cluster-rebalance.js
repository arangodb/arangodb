/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global assertTrue, assertEqual, assertNotEqual, arango */
'use strict';
// //////////////////////////////////////////////////////////////////////////////
// / @brief ArangoDB Enterprise License Tests
// /
// /
// / DISCLAIMER
// /
// / Copyright 2022 ArangoDB Inc, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB Inc, Cologne, Germany
// /
// / @author Lars Maier
// //////////////////////////////////////////////////////////////////////////////

let jsunity = require('jsunity');
const arangodb = require('@arangodb');
const db = arangodb.db;
const { getDBServers } = require('@arangodb/test-helper');
const database = "cluster_rebalance_db";
let prevDB = null;

function resignServer(server) {
  let res = arango.POST_RAW("/_admin/cluster/resignLeadership", { server });
  assertEqual(202, res.code);
  const id = res.parsedBody.id;

  let count = 10;
  while (--count >= 0) {
    require("internal").wait(5.0, false);
    res = arango.GET_RAW("/_admin/cluster/queryAgencyJob?id=" + id);
    if (res.code === 200 && res.parsedBody.status === "Finished") {
      return;
    }
  }
  assertTrue(false, `We failed to resign a leader in 50s. We cannot reliably test rebalancing of shards now.`);
}

function clusterRebalanceSuite() {
  return {
    setUpAll: function () {
      prevDB = db._name();
      db._createDatabase(database);
      db._useDatabase(database);
      for (let i = 0; i < 20; i++) {
        db._create("col" + i, {replicationFactor: 2});
      }
      // resign one server
      resignServer(getDBServers()[0].id);
    },

    tearDownAll: function () {
      for (let i = 0; i < 20; i++) {
        db._drop("col" + i);
      }
      db._useDatabase(prevDB);
      db._dropDatabase(database);
    },

    testGetImbalance: function () {
      let result = arango.GET('/_admin/cluster/rebalance');
      assertEqual(result.code, 200);
      assertEqual(result.error, false);
    },
    testCalcRebalanceVersion: function () {
      let result = arango.POST('/_admin/cluster/rebalance', {
        version: 3
      });
      assertEqual(result.code, 400);
    },
    testCalcRebalance: function () {
      let result = arango.POST('/_admin/cluster/rebalance', {
        version: 1,
        moveLeaders: true,
        moveFollowers: true,
        leaderChanges: true,
        databasesExcluded: ["_system"],
      });
      assertEqual(result.code, 200); // should be refused
      assertEqual(result.error, false); // should be refused
      const moves = result.result.moves;
      assertTrue(moves.length > 0);
      for (const job of moves) {
        assertNotEqual(job.database, "_system");
      }
    },

    testCalcRebalanceAndExecute: function () {
      let result = arango.POST('/_admin/cluster/rebalance', {
        version: 1,
        moveLeaders: true,
        moveFollowers: true,
        leaderChanges: true,
        databasesExcluded: ["_system"],
      });
      assertEqual(result.code, 200);
      assertEqual(result.error, false);
      let moves = result.result.moves;
      assertTrue(moves.length > 0);
      for (const job of moves) {
        assertNotEqual(job.database, "_system");
      }

      result = arango.POST('/_admin/cluster/rebalance/execute', {version: 1, moves});
      assertEqual(result.code, 202);
      assertEqual(result.error, false);

      // empty set of moves
      result = arango.POST('/_admin/cluster/rebalance/execute', {version: 1, moves: []});
      assertEqual(result.code, 200);
      assertEqual(result.error, false);

      while (true) {
        require('internal').wait(0.5);
        let result = arango.GET('/_admin/cluster/rebalance');
        assertEqual(result.code, 200);
        assertEqual(result.error, false);

        if (result.result.pendingMoveShards === 0 && result.result.todoMoveShards === 0) {
          break;
        }
      }
    },

    testExecuteRebalanceVersion: function () {
      let result = arango.POST('/_admin/cluster/rebalance/execute', {
        version: 3, moves: []
      });
      assertEqual(result.code, 400);
    },
  };
}

jsunity.run(clusterRebalanceSuite);
return jsunity.done();
