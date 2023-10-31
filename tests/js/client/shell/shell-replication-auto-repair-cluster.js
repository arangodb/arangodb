/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertEqual, assertNotEqual, assertFalse, assertTrue */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
// / @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////

let jsunity = require('jsunity');
let internal = require('internal');
let arangodb = require('@arangodb');
let db = arangodb.db;
let request = require("@arangodb/request");
let errors = arangodb.errors;
let { getEndpointById,
      getEndpointsByType,
      debugCanUseFailAt,
      debugSetFailAt,
      debugClearFailAt,
      getChecksum,
      getMetric
    } = require('@arangodb/test-helper');
  
const cn = 'UnitTestsReplication';
  
let getBatch = (ep) => {
  let result = request({ 
    method: "POST", 
    url: ep + "/_api/replication/batch",
    body: { ttl: 60 },
    json: true
  });
  assertEqual(200, result.status);
  return result.json.id;
};
    
let deleteBatch = (ep, batchId) => {
  let result = request({ method: "DELETE", url: ep + "/_api/replication/batch/" + batchId });
  assertEqual(204, result.status);
};
    
let getTree = (ep, batchId, shardId) => {
  let result = request({ 
    method: "GET", 
    url: ep + "/_api/replication/revisions/tree?collection=" + encodeURIComponent(shardId) + "&verification=true&onlyPopulated=true&batchId=" + batchId 
  });
  assertEqual(200, result.status);
  return result.json;
};

function assertInSync(leader, follower, shardId) {
  // compare trees of leader and follower
  let leaderEqual = false;
  let followerEqual = false;
  let tries = 0;
  while (++tries < 300) {
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
      endpoints.push(getEndpointById(s));
    });
    return [shardId, endpoints];
  };

  let logLevels = {};

  return {
    setUp: function () {
      getEndpointsByType("dbserver").forEach((ep) => {
        debugClearFailAt(ep);
      
        // store original log levels
        let result = request({ method: "GET", url: ep + "/_admin/log/level" });
        logLevels[ep] = result.json;
        // adjust log level for replication topic
        request({ method: "PUT", url: ep + "/_admin/log/level", body: { replication: "debug", maintenance: "info" }, json: true });
      });
    },

    tearDown: function () {
      getEndpointsByType("dbserver").forEach((ep) => {
        debugClearFailAt(ep);
        // restore original log level
        request({ method: "PUT", url: ep + "/_admin/log/level", body: logLevels[ep], json: true });
      });
      db._drop(cn);
    },
    
    testAutoRepairWhenTreeBrokenOnLeader: function () {
      let c = db._create(cn, { numberOfShards: 1, replicationFactor: 2 });
      let [shardId, [leader, follower]] = getEndpoints();

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
          debugSetFailAt(leader, "replicateOperations::skip");
        }
        c.insert(docs);
      }
     
      // verify document counts
      {
        // leader should have all the documents
        let result = request({ method: "GET", url: leader + "/_api/collection/" + shardId + "/count" });
        assertEqual(200, result.status);
        assertEqual(n, result.json.count);
        // follower should have only half the documents
        result = request({ method: "GET", url: follower + "/_api/collection/" + shardId + "/count" });
        assertEqual(200, result.status);
        assertEqual(n / 2, result.json.count);
      }
      
      // corrupt the leader's revision tree
      {
        // first make sure that the leader has a tree for the shard
        let batchId = getBatch(leader);
        let result = getTree(leader, batchId, shardId);
        deleteBatch(leader, batchId);

        // and now corrupt it
        result = request({ method: "PUT", url: leader + "/_api/replication/revisions/tree?collection=" + encodeURIComponent(shardId) + "&count=1234&hash=42" });
        assertEqual(200, result.status);
      }
      
      debugClearFailAt(leader);

      // this will trigger a drop-follower operation on the next insert on the leader
      debugSetFailAt(leader, "replicateOperationsDropFollower");
     
      // enable sending of revision-tree data from follower to leader for comparison
      debugSetFailAt(follower, "synchronizeShardSendTreeData");

      // disable intentional delays of subsequent replication attempts on follower
      debugSetFailAt(follower, "SynchronizeShard::noSleepOnSyncError");

      let leaderRebuildsBefore = getMetric(leader, "arangodb_sync_tree_rebuilds_total");
      let droppedFollowersBefore = getMetric(leader, "arangodb_dropped_followers_total");

      // insert a single document. this will drop the follower, and trigger a resync. 
      // the follower will need to get in sync using the incremental sync protocol
      c.insert({});
      
      debugClearFailAt(leader);
    
      // follower must have been dropped by the insert
      let droppedFollowersAfter = getMetric(leader, "arangodb_dropped_followers_total");
      assertTrue(droppedFollowersBefore < droppedFollowersAfter, { droppedFollowersBefore, droppedFollowersAfter });
      
      // wait for shards to get in sync and revision trees to get repaired
      assertInSync(leader, follower, shardId);

      // we must have seen one tree rebuild on the leader
      let leaderRebuildsAfter = getMetric(leader, "arangodb_sync_tree_rebuilds_total");
      assertTrue(leaderRebuildsBefore < leaderRebuildsAfter, { leaderRebuildsBefore, leaderRebuildsAfter });
    },
    
    testAutoRepairWhenTreeBrokenOnFollower: function () {
      let c = db._create(cn, { numberOfShards: 1, replicationFactor: 2 });
      let [shardId, [leader, follower]] = getEndpoints();

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
          debugSetFailAt(leader, "replicateOperations::skip");
        }
        c.insert(docs);
      }
     
      // verify document counts
      {
        // leader should have all the documents
        let result = request({ method: "GET", url: leader + "/_api/collection/" + shardId + "/count" });
        assertEqual(200, result.status);
        assertEqual(n, result.json.count);
        // follower should have only half the documents
        result = request({ method: "GET", url: follower + "/_api/collection/" + shardId + "/count" });
        assertEqual(200, result.status);
        assertEqual(n / 2, result.json.count);
      }
      
      debugClearFailAt(leader);
    
      // corrupt the followers's revision tree
      {
        // first make sure that the follower has a tree for the shard
        let batchId = getBatch(follower);
        let result = getTree(follower, batchId, shardId);
        deleteBatch(follower, batchId);

        // and now corrupt it
        result = request({ method: "PUT", url: follower + "/_api/replication/revisions/tree?collection=" + encodeURIComponent(shardId) + "&count=1234&hash=42" });
        assertEqual(200, result.status);
      }

      // this will trigger a drop-follower operation on the next insert on the leader
      debugSetFailAt(leader, "replicateOperationsDropFollower");
     
      // enable sending of revision-tree data from follower to leader for comparison
      debugSetFailAt(follower, "synchronizeShardSendTreeData");

      // disable intentional delays of subsequent replication attempts on follower
      debugSetFailAt(follower, "SynchronizeShard::noSleepOnSyncError");

      let followerRebuildsBefore = getMetric(follower, "arangodb_sync_tree_rebuilds_total");
      let droppedFollowersBefore = getMetric(leader, "arangodb_dropped_followers_total");

      // insert a single document. this will drop the follower, and trigger a resync. 
      // the follower will need to get in sync using the incremental sync protocol
      c.insert({});
      
      debugClearFailAt(leader);
    
      // follower must have been dropped by the insert
      let droppedFollowersAfter = getMetric(leader, "arangodb_dropped_followers_total");
      assertTrue(droppedFollowersBefore < droppedFollowersAfter, { droppedFollowersBefore, droppedFollowersAfter });
      
      // wait for shards to get in sync and revision trees to get repaired
      assertInSync(leader, follower, shardId);

      // we must have seen one tree rebuild on the follower
      let followerRebuildsAfter = getMetric(follower, "arangodb_sync_tree_rebuilds_total");
      assertTrue(followerRebuildsBefore < followerRebuildsAfter, { followerRebuildsBefore, followerRebuildsAfter });
    },
      
  };
}

let ep = getEndpointsByType('dbserver');
if (ep.length && debugCanUseFailAt(ep[0])) {
  // only execute if failure tests are available
  jsunity.run(replicationAutoRepairSuite);
}
return jsunity.done();
