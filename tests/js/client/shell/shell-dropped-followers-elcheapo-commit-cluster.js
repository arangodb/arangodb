/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertEqual, assertTrue, assertFalse, arango */

// //////////////////////////////////////////////////////////////////////////////
// / @brief ArangoTransaction el cheapo and dropped followers
// /
// /
// / DISCLAIMER
// /
// / Copyright 2021 ArangoDB GmbH, Cologne, Germany
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
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Max Neunhoeffer
// //////////////////////////////////////////////////////////////////////////////

let jsunity = require('jsunity');
let internal = require('internal');
let arangodb = require('@arangodb');
let db = arangodb.db;
let errors = arangodb.errors;
let { getEndpointById,
      getEndpointsByType,
      debugCanUseFailAt,
      debugSetFailAt,
      debugClearFailAt
    } = require('@arangodb/test-helper');

function dropFollowersElCheapoSuite() {
  'use strict';
  const cn = 'UnitTestsElCheapoDroppedFollowers';

  return {

    setUp: function () {
      getEndpointsByType("dbserver").forEach((ep) => debugClearFailAt(ep));
      db._drop(cn);
      db._create(cn, {numberOfShards:2, replicationFactor:2});
    },

    tearDown: function () {
      getEndpointsByType("dbserver").forEach((ep) => debugClearFailAt(ep));
      db._drop(cn);
    },
    
    testDropFollowerDuringTransactionMultipleShards: function() {
      // Get dbserver names first:
      let health = arango.GET("/_admin/cluster/health").Health;
      let endpointMap = {};
      for (let sid in health) {
        endpointMap[health[sid].ShortName] = health[sid].Endpoint;
      }
      let plan = arango.GET("/_admin/cluster/shardDistribution").results[cn].Plan;
      let shards = Object.keys(plan);
      let coordinator = "Coordinator0001";
      let leader = plan[shards[0]].leader;
      let follower = plan[shards[0]].followers[0];
      if (leader !== plan[shards[1]].leader) {
        let moveShardJob = {
          database: "_system",
          collection: cn,
          shard: shards[1],
          fromServer: plan[shards[1]].leader,
          toServer: leader,
          isLeader: true
        };
        let res = arango.POST("/_admin/cluster/moveShard", moveShardJob);
        while (true) {
          let res2 = arango.GET(`/_admin/cluster/queryAgencyJob?id=${res.id}`);
          if (res2.status === "Finished") {
            break;
          }
          internal.wait(1);
        }
      }
      plan = arango.GET("/_admin/cluster/shardDistribution").results[cn].Plan;
      if (follower !== plan[shards[1]].followers[0]) {
        let moveShardJob = {
          database: "_system",
          collection: cn,
          shard: shards[1],
          fromServer: plan[shards[1]].followers[0],
          toServer: follower,
          isLeader: false
        };
        let res = arango.POST("/_admin/cluster/moveShard", moveShardJob);
        while (true) {
          let res2 = arango.GET(`/_admin/cluster/queryAgencyJob?id=${res.id}`);
          if (res2.status === "Finished") {
            break;
          }
          internal.wait(1);
        }
      }
      plan = arango.GET("/_admin/cluster/shardDistribution").results[cn].Plan;

      // Now we have two shards whose leader and follower are the same.
      
      // Let's insert some documents:

      // Now run one transaction touching multiple documents:
      let trx = arango.POST("/_api/transaction/begin", {collections:{write:[cn]}});
      let trxid = trx.result.id;
      for (let i = 0; i < 20; ++i) {
        arango.POST("/_api/document/" + cn, {_key:"A"+i},
                    {"x-arango-trx-id":trxid});
      }

      // Now the follower is in the knownServers list and each shard has
      // got some documents written with a high likelyhood
      // Now activate a failure point on the leader to make it drop a follower
      // with the next request:
      arango.reconnect(endpointMap[leader], "_system", "root", "");
      arango.PUT("/_admin/debug/failat/replicateOperationsDropFollower",{});
      arango.reconnect(endpointMap[coordinator], "_system", "root", "");

      arango.POST("/_api/document/" + cn, {_key:"F"},
                  {"x-arango-trx-id":trxid});

      arango.reconnect(endpointMap[leader], "_system", "root", "");
      arango.DELETE("/_admin/debug/failat/replicateOperationsDropFollower");
      arango.reconnect(endpointMap[coordinator], "_system", "root", "");

      let commitRes = arango.PUT(`/_api/transaction/${trxid}`, {});
      assertFalse(commitRes.error);

      // Now check that no subordinate transaction is still running on the
      // leader or follower:
      arango.reconnect(endpointMap[leader],"_system", "root", "");
      let trxsLeader = arango.GET("/_api/transaction");
      assertTrue(trxsLeader.hasOwnProperty("transactions"));
      let followerTrxId = String(Number(trxid) + 2);
      let found = false;
      for (let t of trxsLeader.transactions) {
        if (t.id === followerTrxId) {
          found = true;
        }
      }
      assertFalse(found);
      arango.reconnect(endpointMap[follower],"_system", "root", "");
      let trxsFollower = arango.GET("/_api/transaction");
      assertTrue(trxsFollower.hasOwnProperty("transactions"));
      for (let t of trxsFollower.transactions) {
        if (t.id === followerTrxId) {
          found = true;
        }
      }
      assertFalse(found);

      // The above transaction should be entirely visible across the shards,
      // note that we need to check on the dbservers and not via the,
      // coordinator otherwise we do not see the follower information.

      arango.reconnect(endpointMap[leader],"_system", "root", "");
      let count = 0;
      for (let s of shards) {
        count += db._collection(s).count();
      }
      assertEqual(21, count);

      arango.reconnect(endpointMap[follower],"_system", "root", "");
      count = 0;
      for (let s of shards) {
        count += db._collection(s).count();
      }
      assertEqual(21, count);

      arango.reconnect(endpointMap[coordinator], "_system", "root", "");
    },
    
  };
}

let ep = getEndpointsByType('dbserver');
if (ep.length && debugCanUseFailAt(ep[0])) {
  // only execute if failure tests are available
  jsunity.run(dropFollowersElCheapoSuite);
}
return jsunity.done();
