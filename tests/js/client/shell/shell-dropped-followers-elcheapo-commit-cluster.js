/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertEqual, assertTrue, assertFalse, arango */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Max Neunhoeffer
// //////////////////////////////////////////////////////////////////////////////

const _ = require('lodash');
let jsunity = require('jsunity');
let internal = require('internal');
let arangodb = require('@arangodb');
let db = arangodb.db;
let { getEndpointsByType,
      debugCanUseFailAt,
      debugClearFailAt,
      waitForShardsInSync
    } = require('@arangodb/test-helper');

    
function getEndpointMap() {
  const health = arango.GET("/_admin/cluster/health").Health;
  const endpointMap = {};
  for (let sid in health) {
    endpointMap[health[sid].ShortName] = health[sid].Endpoint;
  }
  return endpointMap;
}

function createCollectionWithTwoShardsSameLeaderAndFollower(cn) {
  db._create(cn, {numberOfShards:2, replicationFactor:2});
  // Get dbserver names first:
  const endpointMap = getEndpointMap();
  let plan = arango.GET("/_admin/cluster/shardDistribution").results[cn].Plan;
  let shards = Object.keys(plan);
  let coordinator = "Coordinator0001";
  let leader = plan[shards[0]].leader;
  let follower = plan[shards[0]].followers[0];
  // Make leaders the same:
  if (leader !== plan[shards[1]].leader) {
    let moveShardJob = {
      database: db._name(),
      collection: cn,
      shard: shards[1],
      fromServer: plan[shards[1]].leader,
      toServer: leader,
      isLeader: true
    };
    let res = arango.POST("/_admin/cluster/moveShard", moveShardJob);
    let start = internal.time();
    while (true) {
      if (internal.time() - start > 120) {
        assertTrue(false, "timeout waiting for shards being in sync");
        return;
      }
      let res2 = arango.GET(`/_admin/cluster/queryAgencyJob?id=${res.id}`);
      if (res2.status === "Finished") {
        break;
      }
      internal.wait(1);
    }
    // Now we have to wait until the Plan has only one follower again, otherwise
    // the second moveShard operation can fail and thus the test would be
    // vulnerable to bad timing (as has been seen on Windows):
    start = internal.time();
    while (true) {
      if (internal.time() - start > 120) {
        assertTrue(false, "timeout waiting for shards being in sync");
        return;
      }
      plan = arango.GET("/_admin/cluster/shardDistribution").results[cn].Plan;
      if (plan[shards[1]].followers.length === 1) {
        break;
      }
      internal.wait(1);
    }
  }
  // Make followers the same:
  if (follower !== plan[shards[1]].followers[0]) {
    let moveShardJob = {
      database: db._name(),
      collection: cn,
      shard: shards[1],
      fromServer: plan[shards[1]].followers[0],
      toServer: follower,
      isLeader: false
    };
    let res = arango.POST("/_admin/cluster/moveShard", moveShardJob);
    let start = internal.time();
    while (true) {
      if (internal.time() - start > 120) {
        assertTrue(false, "timeout waiting for shards being in sync");
        return;
      }
      let res2 = arango.GET(`/_admin/cluster/queryAgencyJob?id=${res.id}`);
      if (res2.status === "Finished") {
        break;
      }
      internal.wait(1);
    }
  }
  return { endpointMap, coordinator, leader, follower, shards };
}

function switchConnectionToCoordinator(collInfo) {
  arango.reconnect(collInfo.endpointMap[collInfo.coordinator], db._name(), "root", "");
}

function switchConnectionToLeader(collInfo) {
  arango.reconnect(collInfo.endpointMap[collInfo.leader], db._name(), "root", "");
}

function switchConnectionToFollower(collInfo) {
  arango.reconnect(collInfo.endpointMap[collInfo.follower], db._name(), "root", "");
}

function dropFollowersElCheapoSuite() {
  'use strict';
  const cn = 'UnitTestsElCheapoDroppedFollowers';
  let collInfo = {};

  const endpointMap = getEndpointMap();
  const info = { endpointMap, coordinator: "Coordinator0001" };

  return {
    setUp: function () {
      switchConnectionToCoordinator(info);
      getEndpointsByType("dbserver").forEach((ep) => debugClearFailAt(ep));
      db._drop(cn);
      collInfo = createCollectionWithTwoShardsSameLeaderAndFollower(cn);
    },

    tearDown: function () {
      switchConnectionToCoordinator(info);
      getEndpointsByType("dbserver").forEach((ep) => debugClearFailAt(ep));
      db._drop(cn);
    },
    
    testDropFollowerDuringTransactionMultipleShards: function() {
      // We have two shards whose leader is the same and whose follower is
      // the same.
      
      // Let's insert some documents:

      // Run one transaction touching multiple documents:
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
      switchConnectionToLeader(collInfo);
      arango.PUT("/_admin/debug/failat/replicateOperationsDropFollower",{});
      switchConnectionToCoordinator(collInfo);

      arango.POST("/_api/document/" + cn, {_key:"F"},
                  {"x-arango-trx-id":trxid});

      switchConnectionToLeader(collInfo);
      arango.DELETE("/_admin/debug/failat/replicateOperationsDropFollower");
      switchConnectionToCoordinator(collInfo);

      let commitRes = arango.PUT(`/_api/transaction/${trxid}`, {});
      assertFalse(commitRes.error);

      // Now check that no subordinate transaction is still running on the
      // leader or follower:
      switchConnectionToLeader(collInfo);
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
      switchConnectionToFollower(collInfo);
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

      switchConnectionToCoordinator(collInfo);
      waitForShardsInSync(cn, 60, 1);

      switchConnectionToLeader(collInfo);
      let count = _.sumBy(collInfo.shards, s => db._collection(s).count());
      assertEqual(21, count);

      switchConnectionToFollower(collInfo);
      count = _.sumBy(collInfo.shards, s => db._collection(s).count());
      assertEqual(21, count);

      switchConnectionToCoordinator(collInfo);
    },
    
    testDropFollowerThenFailCommit : function() {
      // We have two shards whose leader is the same and whose follower is
      // the same.
      
      // Let's insert some documents:

      // Run one transaction touching multiple documents:
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
      switchConnectionToLeader(collInfo);
      arango.PUT("/_admin/debug/failat/replicateOperationsDropFollower",{});
      switchConnectionToCoordinator(collInfo);

      // And another 20, then both shards should be dropped:
      for (let i = 0; i < 20; ++i) {
        arango.POST("/_api/document/" + cn, {_key:"B"+i},
                    {"x-arango-trx-id":trxid});
      }

      switchConnectionToLeader(collInfo);
      arango.DELETE("/_admin/debug/failat/replicateOperationsDropFollower");
      switchConnectionToCoordinator(collInfo);

      // Now fail the commit on the follower:
      switchConnectionToFollower(collInfo);
      arango.PUT("/_admin/debug/failat/TransactionCommitFail",{});
      switchConnectionToCoordinator(collInfo);

      let commitRes = arango.PUT(`/_api/transaction/${trxid}`, {});
      assertFalse(commitRes.error);

      switchConnectionToFollower(collInfo);
      arango.DELETE("/_admin/debug/failat/TransactionCommitFail");
      switchConnectionToCoordinator(collInfo);

      // Now check that no subordinate transaction is still running on the
      // leader or follower:
      switchConnectionToLeader(collInfo);
      let trxsLeader = arango.GET("/_api/transaction");
      assertTrue(trxsLeader.hasOwnProperty("transactions"));
      let followerTrxId = String(Number(trxid) + 2);
      let found = trxsLeader.transactions.some(t => t.id === followerTrxId);
      assertFalse(found);
      switchConnectionToFollower(collInfo);
      let trxsFollower = arango.GET("/_api/transaction");
      assertTrue(trxsFollower.hasOwnProperty("transactions"));
      found = trxsFollower.transactions.some(t => t.id === followerTrxId);
      assertFalse(found);

      // The above transaction should be entirely visible across the shards,
      // note that we need to check on the dbservers and not via the,
      // coordinator otherwise we do not see the follower information.

      switchConnectionToCoordinator(collInfo);
      waitForShardsInSync(cn, 60, 1);

      switchConnectionToLeader(collInfo);
      let count = _.sumBy(collInfo.shards, s => db._collection(s).count());
      assertEqual(40, count);

      switchConnectionToFollower(collInfo);
      count = _.sumBy(collInfo.shards, s => db._collection(s).count());
      assertEqual(40, count);

      switchConnectionToCoordinator(collInfo);
    },
    
  };
}

function lockTimeoutSuite() {
  'use strict';
  const cn = 'UnitTestsLockTimeout';
  let collInfo = {};

  const endpointMap = getEndpointMap();
  const info = { endpointMap, coordinator: "Coordinator0001" };

  return {
    setUp: function () {
      switchConnectionToCoordinator(info);
      getEndpointsByType("dbserver").forEach((ep) => debugClearFailAt(ep));
      db._createDatabase(cn);
      db._useDatabase(cn);
      collInfo = createCollectionWithTwoShardsSameLeaderAndFollower(cn);
    },

    tearDown: function () {
      switchConnectionToCoordinator(info);
      getEndpointsByType("dbserver").forEach((ep) => debugClearFailAt(ep));
      db._useDatabase('_system');
      db._dropDatabase(cn);
    },
    
    testLockTimeouts: function() {
      // All we want to do is a single query and for that switch on some
      // assertions:

      switchConnectionToLeader(collInfo);
      arango.PUT("/_admin/debug/failat/assertLockTimeoutLow",{});
      switchConnectionToFollower(collInfo);
      arango.PUT("/_admin/debug/failat/assertLockTimeoutHigh",{});
      switchConnectionToCoordinator(collInfo);

      // we are not testing much inside this test here, except that the
      // DB servers don't run into the assertion failure in RocksDBMetaCollection
      // while trying to acquire the collection lock.
      db._query(`FOR i IN 1..100 INSERT {Hallo:i} INTO ${cn} RETURN NEW`).toArray();
    },
  };
}

if (db._properties().replicationVersion !== "2") {
  jsunity.run(dropFollowersElCheapoSuite);
}
jsunity.run(lockTimeoutSuite);
return jsunity.done();
