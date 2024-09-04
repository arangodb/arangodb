/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertEqual, assertNotEqual, assertFalse, assertTrue */

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
      getChecksum,
      getMetric
    } = require('@arangodb/test-helper');
let { instanceRole } = require('@arangodb/testutils/instance');
let IM = global.instanceManager;

const cn = 'UnitTestsTransaction';

function assertInSync(leader, follower, shardId) {
  const leaderChecksum = getChecksum(leader, shardId);
  let followerChecksum;
  let tries = 0;
  while (++tries < 120) {
    followerChecksum = getChecksum(follower, shardId);
    if (followerChecksum === leaderChecksum) {
      break;
    }
    internal.sleep(0.25);
  }
  assertEqual(leaderChecksum, followerChecksum);
}

function replicationIntermediateCommitsSuite() {
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

  return {
    setUp: function () {
      IM.debugClearFailAt('', instanceRole.dbServer);
    },

    tearDown: function () {
      IM.debugClearFailAt('', instanceRole.dbServer);
      db._drop(cn);
    },
    
    testFollowerDoesIntermediateCommits: function () {
      if (db._properties().replicationVersion === "2") {
        // this test is only relevant for replication 1
        return;
      }
      let c = db._create(cn, { numberOfShards: 1, replicationFactor: 2 });
      let [shardId, [leader, follower]] = getEndpoints();

      let docs = [];
      for (let i = 0; i < 5000; ++i) {
        docs.push({ value1: i, value2: "testmann" + i });
      }

      // insert docs, but only half of them on the follower
      const n = 250 * 1000;
      let count = 0;
      while (true) {
        count = c.count();
        if (count >= n) {
          break;
        }
        if (count === n / 2) {
          // do not replicate from leader to follower after half of documents
          IM.debugSetFailAt("replicateOperations::skip", instanceRole.dbServer, leader);
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
      
      IM.debugClearFailAt('', instanceRole.dbServer, leader);
      // this will trigger a drop-follower operation on the next insert on the leader
      IM.debugSetFailAt("replicateOperationsDropFollower", instanceRole.dbServer, leader);

      let intermediateCommitsBefore = getMetric(follower, "arangodb_intermediate_commits_total");
      let droppedFollowersBefore = getMetric(leader, "arangodb_dropped_followers_total");

      // insert a single document. this will drop the follower, and trigger a resync. 
      // the follower will need to get in sync using the incremental sync protocol
      c.insert({});
      
      let droppedFollowersAfter = getMetric(leader, "arangodb_dropped_followers_total");
      assertTrue(droppedFollowersBefore < droppedFollowersAfter, { droppedFollowersBefore, droppedFollowersAfter });
      
      assertInSync(leader, follower, shardId);

      let intermediateCommitsAfter = getMetric(leader, "arangodb_intermediate_commits_total");
      // we expect an intermediate commit for every 10k documents.
      let expected = (n / 2 / 10000); 
      assertTrue(intermediateCommitsBefore < intermediateCommitsAfter + expected, { intermediateCommitsBefore, intermediateCommitsAfter, expected });
    },
      
  };
}

jsunity.run(replicationIntermediateCommitsSuite);
return jsunity.done();
