/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global arango, fail, assertEqual, assertNotEqual, assertFalse, assertTrue */

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

let jsunity = require('jsunity');
let internal = require('internal');
let arangodb = require('@arangodb');
let db = arangodb.db;
let errors = arangodb.errors;
let { versionHas } = require('@arangodb/test-helper');
const isCov = versionHas('coverage');
let { instanceRole } = require('@arangodb/testutils/instance');
let IM = global.instanceManager;

const cn = 'UnitTestsReplication';
  
let getBatch = (arangod) => {
  return arangod.toThisInstance(() => {
    let result = arango.POST_RAW("/_api/replication/batch", { ttl: 60 });
    assertEqual(200, result.code);
    return result.parsedBody.id;
  });
};
    
let deleteBatch = (arangod, batchId) => {
  arangod.toThisInstance(() => {
    let result = arango.DELETE("/_api/replication/batch/" + batchId);
    assertEqual(204, result.code);
  });
};
    
let getTree = (arangod, batchId, shardId) => {
  return arangod.toThisInstance(() => {
    let result = arango.GET_RAW("/_api/replication/revisions/tree?collection=" + encodeURIComponent(shardId) + "&verification=true&onlyPopulated=true&batchId=" + batchId);
    assertEqual(200, result.code);
    return result.parsedBody;
  });
};

function assertInSync(leader, follower, shardId) {
  // compare trees of leader and follower
  let leaderEqual = false;
  let followerEqual = false;
  let tries = 0;
  const maxTries = isCov ? 600 : 300;
  while (++tries < maxTries) {
    let batchId = getBatch(leader);
    let result = getTree(leader, batchId, shardId);
    deleteBatch(leader, batchId);

    if (result.equal) {
      leaderEqual = true;
    
      let batchId = getBatch(follower);
      let result = getTree(follower, batchId, shardId);
      deleteBatch(follower, batchId);
      
      if (result.equal) {
        followerEqual = true;
        break;
      }
    }
    internal.sleep(1);
  }
  assertTrue(leaderEqual);
  assertTrue(followerEqual);
}

function replicationAutoRepairSuite() {
  'use strict';
  let getEndpoints = () => {
    let shards = db._collection(cn).shards(true);
    let shardId = Object.keys(shards)[0];
    let endpoints = [];
    shards[shardId].forEach((s) => {
      endpoints.push(s);
    });
    return [shardId, endpoints];
  };

  let logLevels = {};

  return {
    setUp: function () {
      IM.debugClearFailAt('', instanceRole.dbServer);
      IM.getInstancesRole(instanceRole.dbserver).forEach((arangod) => {
        arangod.toThisInstance(() => {
          // store original log levels
          let result = arango.GET_RAW("/_admin/log/level");
          logLevels[arangod.id] = result.parsedBody;
          // adjust log level for replication topic
          arango.PUT_RAW("/_admin/log/level", { replication: "debug", maintenance: "info" });
        });
      });
    },

    tearDown: function () {
      IM.debugClearFailAt('', instanceRole.dbServer);
      IM.getInstancesRole(instanceRole.dbserver).forEach((arangod) => {
        // restore original log level
        arangod.toThisInstance(() => {
          arango.PUT_RAW("/_admin/log/level", logLevels[arangod.id]);
        });
      });
      db._drop(cn);
    },
    
    testAutoRepairWhenTreeBrokenOnLeader: function () {
      let c = db._create(cn, { numberOfShards: 1, replicationFactor: 2 });
      let [shardId, [leaderID, followerID]] = getEndpoints();
      let leader = IM.getInstanceByID(leaderID);
      let follower = IM.getInstanceByID(followerID);

      let docs = [];
      for (let i = 0; i < 5000; ++i) {
        docs.push({ value1: i, value2: "testmann" + i });
      }

      // insert docs, but only half of them on the follower
      const n = 60 * 1000;
      let count = 0;
      while (true) {
        count = c.count();
        if (count >= n) {
          break;
        }
        if (count === n / 2) {
          // do not replicate from leader to follower after half of documents
          leader.debugSetFailAt("replicateOperations::skip");
        }
        c.insert(docs);
      }
     
      // verify document counts
      {
        // leader should have all the documents
        let result = leader.toThisInstance(() => {
          return arango.GET_RAW("/_api/collection/" + shardId + "/count" );
        });
        assertEqual(200, result.code);
        assertEqual(n, result.parsedBody.count);
        // follower should have only half the documents
        result = follower.toThisInstance(() => {
          return arango.GET_RAW("/_api/collection/" + shardId + "/count");
        });
        assertEqual(200, result.code);
        assertEqual(n / 2, result.parsedBody.count);
      }
      
      // corrupt the leader's revision tree
      {
        // first make sure that the leader has a tree for the shard
        let batchId = getBatch(leader);
        let result = getTree(leader, batchId, shardId);
        deleteBatch(leader, batchId);

        // and now corrupt it
        result = leader.toThisInstance(() => {
          return arango.PUT_RAW("/_api/replication/revisions/tree?collection=" + encodeURIComponent(shardId) + "&count=1234&hash=42", '');
        });
        assertEqual(200, result.code);
      }
      
      leader.debugClearFailAt();

      // this will trigger a drop-follower operation on the next insert on the leader
      leader.debugSetFailAt("replicateOperationsDropFollower");
     
      // enable sending of revision-tree data from follower to leader for comparison
      follower.debugSetFailAt("synchronizeShardSendTreeData");

      // disable intentional delays of subsequent replication attempts on follower
      follower.debugSetFailAt("SynchronizeShard::noSleepOnSyncError");

      let leaderRebuildsBefore = leader.getMetric("arangodb_sync_tree_rebuilds_total");
      let droppedFollowersBefore = leader.getMetric("arangodb_dropped_followers_total");

      // insert a single document. this will drop the follower, and trigger a resync. 
      // the follower will need to get in sync using the incremental sync protocol
      c.insert({});
      
      IM.debugClearFailAt('', instanceRole.dbServer, leader);
    
      // follower must have been dropped by the insert
      let droppedFollowersAfter = leader.getMetric("arangodb_dropped_followers_total");
      assertTrue(droppedFollowersBefore < droppedFollowersAfter, { droppedFollowersBefore, droppedFollowersAfter });
      
      // wait for shards to get in sync and revision trees to get repaired
      assertInSync(leader, follower, shardId);

      // we must have seen one tree rebuild on the leader
      let leaderRebuildsAfter = leader.getMetric("arangodb_sync_tree_rebuilds_total");
      assertTrue(leaderRebuildsBefore < leaderRebuildsAfter, { leaderRebuildsBefore, leaderRebuildsAfter });
    },
    
    testAutoRepairWhenTreeBrokenOnFollower: function () {
      let c = db._create(cn, { numberOfShards: 1, replicationFactor: 2 });
      let [shardId, [leaderID, followerID]] = getEndpoints();
      let leader = IM.getInstanceByID(leaderID);
      let follower = IM.getInstanceByID(followerID);

      let docs = [];
      for (let i = 0; i < 5000; ++i) {
        docs.push({ value1: i, value2: "testmann" + i });
      }

      // insert docs, but only half of them on the follower
      const n = 60 * 1000;
      let count = 0;
      while (true) {
        count = c.count();
        if (count >= n) {
          break;
        }
        if (count === n / 2) {
          // do not replicate from leader to follower after half of documents
          leader.debugSetFailAt("replicateOperations::skip");
        }
        c.insert(docs);
      }
     
      // verify document counts
      {
        // leader should have all the documents
        let result = leader.toThisInstance(() => {
          return arango.GET_RAW("/_api/collection/" + shardId + "/count" );
        });
        assertEqual(200, result.code);
        assertEqual(n, result.parsedBody.count);
        // follower should have only half the documents
        result = follower.toThisInstance(() => {
          return arango.GET_RAW("/_api/collection/" + shardId + "/count");
        });
        assertEqual(200, result.code);
        assertEqual(n / 2, result.parsedBody.count);
      }
      
      leader.debugClearFailAt();
    
      // corrupt the followers's revision tree
      {
        // first make sure that the follower has a tree for the shard
        let batchId = getBatch(follower);
        let result = getTree(follower, batchId, shardId);
        deleteBatch(follower, batchId);

        // and now corrupt it
        result = follower.toThisInstance(() => {
          return arango.PUT_RAW("/_api/replication/revisions/tree?collection=" + encodeURIComponent(shardId) + "&count=1234&hash=42", '');
        });
        assertEqual(200, result.code);
      }

      // this will trigger a drop-follower operation on the next insert on the leader
      leader.debugSetFailAt("replicateOperationsDropFollower");
     
      // enable sending of revision-tree data from follower to leader for comparison
      follower.debugSetFailAt("synchronizeShardSendTreeData");

      // disable intentional delays of subsequent replication attempts on follower
      follower.debugSetFailAt("SynchronizeShard::noSleepOnSyncError");

      let followerRebuildsBefore = follower.getMetric("arangodb_sync_tree_rebuilds_total");
      let droppedFollowersBefore = leader.getMetric("arangodb_dropped_followers_total");

      // insert a single document. this will drop the follower, and trigger a resync. 
      // the follower will need to get in sync using the incremental sync protocol
      c.insert({});
      
      leader.debugClearFailAt();
    
      // follower must have been dropped by the insert
      let droppedFollowersAfter = leader.getMetric("arangodb_dropped_followers_total");
      assertTrue(droppedFollowersBefore < droppedFollowersAfter, { droppedFollowersBefore, droppedFollowersAfter });
      
      // wait for shards to get in sync and revision trees to get repaired
      assertInSync(leader, follower, shardId);

      // we must have seen one tree rebuild on the follower
      let followerRebuildsAfter = follower.getMetric("arangodb_sync_tree_rebuilds_total");
      assertTrue(followerRebuildsBefore < followerRebuildsAfter, { followerRebuildsBefore, followerRebuildsAfter });
    },
      
  };
}

jsunity.run(replicationAutoRepairSuite);
return jsunity.done();
