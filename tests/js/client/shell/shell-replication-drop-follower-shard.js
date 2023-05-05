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
      getMetric,
      waitForShardsInSync
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

function replicationDropFollowerShardSuite() {
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

  let runTest = (failurePoint) => {
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
    // make leader always respond with an error when requesting the revision tree ranges to sync.
    // this will lead to a lot of failed sync attempts
    debugSetFailAt(leader, failurePoint);

    // this will trigger a drop-follower operation on the next insert on the leader
    debugSetFailAt(leader, "replicateOperationsDropFollower");

    // pretend that the leader is a 3.9.2.
    debugSetFailAt(follower, "replicationFakeVersion392");

    // disable intentional delays of subsequent replication attempts on follower
    debugSetFailAt(follower, "SynchronizeShard::noSleepOnSyncError");

    let rebuildFailuresBefore = getMetric(follower, "arangodb_sync_rebuilds_total");
    let droppedFollowersBefore = getMetric(leader, "arangodb_dropped_followers_total");

    // insert a single document. this will drop the follower, and trigger a resync. 
    // the follower will need to get in sync using the incremental sync protocol
    c.insert({});

    // follower must have been dropped by the insert
    let droppedFollowersAfter = getMetric(leader, "arangodb_dropped_followers_total");
    assertTrue(droppedFollowersBefore < droppedFollowersAfter, { droppedFollowersBefore, droppedFollowersAfter });

    // wait for shards to get in sync 
    waitForShardsInSync(cn, 300, 1);

    let rebuildFailuresAfter = getMetric(follower, "arangodb_sync_rebuilds_total");

    assertTrue(rebuildFailuresAfter > rebuildFailuresBefore);
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
    
    testDropFollowerShardAfter20FailedAttemptsBecauseOfRangesErrors: function () {
      // make /_api/replication/tree/ranges intentionally return errors
      runTest("alwaysRefuseRangesRequests");
    },
    
    testDropFollowerShardAfter20FailedAttemptsBecauseOfDocumentsErrors: function () {
      // make /_api/replication/tree/documents intentionally return errors
      runTest("alwaysRefuseDocumentsRequests");
    },
    
  };
}

let ep = getEndpointsByType('dbserver');
if (ep.length && debugCanUseFailAt(ep[0])) {
  // only execute if failure tests are available
  jsunity.run(replicationDropFollowerShardSuite);
}
return jsunity.done();
