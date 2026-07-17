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
// //////////////////////////////////////////////////////////////////////////////

const _ = require('lodash');
let jsunity = require('jsunity');
let internal = require('internal');
let arangodb = require('@arangodb');
let db = arangodb.db;
let { waitForShardsInSync } = require('@arangodb/test-helper');
let { instanceRole } = require('@arangodb/testutils/instance');

let IM = global.instanceManager;

function createCollectionWithTwoShardsSameLeaderAndFollower(cn) {
  db._create(cn, {numberOfShards:2, replicationFactor:2});
  // Get dbserver names first:
  let plan = arango.GET("/_admin/cluster/shardDistribution").results[cn].Plan;
  let shards = Object.keys(plan);
  let coordinator = "Coordinator0001";
  let leader = IM.getInstanceByID(plan[shards[0]].leader);
  let follower = IM.getInstanceByID(plan[shards[0]].followers[0]);
  // Make leaders the same:
  if (leader.id !== plan[shards[1]].leader) {
    let moveShardJob = {
      database: db._name(),
      collection: cn,
      shard: shards[1],
      fromServer: plan[shards[1]].leader,
      toServer: leader.id,
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
  if (follower.id !== plan[shards[1]].followers[0]) {
    let moveShardJob = {
      database: db._name(),
      collection: cn,
      shard: shards[1],
      fromServer: plan[shards[1]].followers[0],
      toServer: follower.id,
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
  return { coordinator, leader, follower, shards };
}

function dropFollowersElCheapoSuite() {
  'use strict';
  const cn = 'UnitTestsElCheapoDroppedFollowers';
  let collInfo = {};

  return {
    setUpAll: function () {
      IM.rememberConnection();
    },

    setUp: function () {
      IM.reconnectMe();
      IM.debugClearFailAt('', instanceRole.dbserver);
      db._drop(cn);
      collInfo = createCollectionWithTwoShardsSameLeaderAndFollower(cn);
    },

    tearDown: function () {
      IM.reconnectMe();
      IM.debugClearFailAt('', instanceRole.dbserver);
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
      collInfo.leader.toThisInstance(() => {
        arango.PUT("/_admin/debug/failat/replicateOperationsDropFollower",{});
      });

      arango.POST("/_api/document/" + cn, {_key:"F"},
                  {"x-arango-trx-id":trxid});

      collInfo.leader.debugClearFailAt("replicateOperationsDropFollower");

      let commitRes = arango.PUT(`/_api/transaction/${trxid}`, {});
      assertFalse(commitRes.error);

      // Now check that no subordinate transaction is still running on the
      // leader or follower:
      let followerTrxId;
      collInfo.leader.toThisInstance(() => {
        let trxsLeader = arango.GET("/_api/transaction");
        assertTrue(trxsLeader.hasOwnProperty("transactions"));
        followerTrxId = String(Number(trxid) + 2);
        let found = false;
        for (let t of trxsLeader.transactions) {
          if (t.id === followerTrxId) {
            found = true;
          }
        }
        assertFalse(found);
      });
      collInfo.follower.toThisInstance(() => {
        let found = false;
        let trxsFollower = arango.GET("/_api/transaction");
        assertTrue(trxsFollower.hasOwnProperty("transactions"));
        for (let t of trxsFollower.transactions) {
          if (t.id === followerTrxId) {
            found = true;
          }
        }
        assertFalse(found);
      });
      
      // The above transaction should be entirely visible across the shards,
      // note that we need to check on the dbservers and not via the,
      // coordinator otherwise we do not see the follower information.

      waitForShardsInSync(cn, 60, 1);

      collInfo.leader.toThisInstance(() => {
        let count = _.sumBy(collInfo.shards, s => db._collection(s).count());
        assertEqual(21, count);
      });

      collInfo.follower.toThisInstance(() => {
        let count = _.sumBy(collInfo.shards, s => db._collection(s).count());
        assertEqual(21, count);
      });
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
      collInfo.leader.debugSetFailAt("replicateOperationsDropFollower");

      // And another 20, then both shards should be dropped:
      for (let i = 0; i < 20; ++i) {
        arango.POST("/_api/document/" + cn, {_key:"B"+i},
                    {"x-arango-trx-id":trxid});
      }

      collInfo.leader.debugClearFailAt("replicateOperationsDropFollower");

      // Now fail the commit on the follower:
      collInfo.follower.debugSetFailAt("TransactionCommitFail");

      let commitRes = arango.PUT(`/_api/transaction/${trxid}`, {});
      assertFalse(commitRes.error);

      collInfo.follower.debugClearFailAt("TransactionCommitFail");

      // Now check that no subordinate transaction is still running on the
      // leader or follower:
      let followerTrxId;
      collInfo.leader.toThisInstance(() => {
        let trxsLeader = arango.GET("/_api/transaction");
        assertTrue(trxsLeader.hasOwnProperty("transactions"));
        followerTrxId = String(Number(trxid) + 2);
        let found = trxsLeader.transactions.some(t => t.id === followerTrxId);
        assertFalse(found);
      });
      collInfo.follower.toThisInstance(() => {
        let trxsFollower = arango.GET("/_api/transaction");
        assertTrue(trxsFollower.hasOwnProperty("transactions"));
        let found = trxsFollower.transactions.some(t => t.id === followerTrxId);
        assertFalse(found);
      });
      // The above transaction should be entirely visible across the shards,
      // note that we need to check on the dbservers and not via the,
      // coordinator otherwise we do not see the follower information.

      waitForShardsInSync(cn, 60, 1);

      collInfo.leader.toThisInstance(() => {
        let count = _.sumBy(collInfo.shards, s => db._collection(s).count());
        assertEqual(40, count);
      });
      collInfo.follower.toThisInstance(() => {
        let count = _.sumBy(collInfo.shards, s => db._collection(s).count());
        assertEqual(40, count);
      });
    },
    
  };
}

function lockTimeoutSuite() {
  'use strict';
  const cn = 'UnitTestsLockTimeout';
  let collInfo = {};

  return {
    setUpAll: function () {
      IM.rememberConnection();
    },

    setUp: function () {
      IM.reconnectMe();
      IM.debugClearFailAt('', instanceRole.dbserver);
      db._createDatabase(cn);
      db._useDatabase(cn);
      collInfo = createCollectionWithTwoShardsSameLeaderAndFollower(cn);
    },

    tearDown: function () {
      IM.reconnectMe();
      IM.debugClearFailAt('', instanceRole.dbserver);
      db._useDatabase('_system');
      db._dropDatabase(cn);
    },
    
    testLockTimeouts: function() {
      // All we want to do is a single query and for that switch on some
      // assertions:

      collInfo.leader.debugSetFailAt("assertLockTimeoutLow");
      collInfo.follower.debugSetFailAt("assertLockTimeoutHigh");

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
