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


function clusterRebalanceSuite() {
  return {
    setUpAll: function () {
      for (let i = 0; i < 10; i++) {
        db._create("col" + i);
      }
    },

    tearDownAll: function () {
      for (let i = 0; i < 10; i++) {
        db._drop("col" + i);
      }
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
        leaderChanges: true
      });
      assertEqual(result.code, 200); // should be refused
      assertEqual(result.error, false); // should be refused
      const moves = result.result.moves;
      assertTrue(moves.length > 0);
    },

    testCalcRebalanceAndExecute: function () {
      let result = arango.POST('/_admin/cluster/rebalance', {
        version: 1,
        moveLeaders: true,
        moveFollowers: true,
        leaderChanges: true
      });
      assertEqual(result.code, 200);
      assertEqual(result.error, false);
      let moves = result.result.moves;
      assertTrue(moves.length > 0);
      result = arango.POST('/_admin/cluster/rebalance/execute', {version: 1, moves});

      // empty set of moves
      result = arango.POST('/_admin/cluster/rebalance/execute', {version: 1, moves: []});
      assertEqual(result.code, 200);
      assertEqual(result.error, false);

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
