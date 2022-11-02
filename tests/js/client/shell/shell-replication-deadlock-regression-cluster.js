/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertTrue, assertFalse, assertEqual,
   assertNotEqual, arango, print */

// //////////////////////////////////////////////////////////////////////////////
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
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Michael Hackstein
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const {db} = require('@arangodb');

const {debugCanUseFailAt} = require("internal");
const {
  getDBServers,
  debugSetFailAt,
  debugClearFailAt,
  getEndpointById,
  waitForShardsInSync
} = require('@arangodb/test-helper');

function ReplicationDeadLockSuite() {
  const collectionName = "UnitTestCollection";
  const clearAllFailurePoints = () => {
    for (const server of getDBServers()) {
      debugClearFailAt(getEndpointById(server.id));
    }
  };

  return {
    setUp: function () {
      // On purpose we use replicationFactor 1 here.
      // We need to increase it in the test
      db._create(collectionName, {replicationFactor: 1});
    },

    tearDown: function () {
      clearAllFailurePoints();
      try {
        db._drop(collectionName);
      } catch (e) {
      }
    },

    testHardLockDoesNotGenerateDeadlocks: function () {
      // This is a regression for BTS-799
      // If we constantly write to a collection, and then increase the replicationFactor
      // at one point so we request an exclusive lock on the collection.
      // We need to ensure the exclusive lock is eventually resolved even if we have high pressure going on.

      // This test simulates the following:
      // 1.) it forces us the use the hard-lock variant to get into sync, which is dangerous as it uses an exclusive lock
      // 2.) On the follower, as soon as we send the hardLock request out, we block MED and LOW lanes and simulate high load
      // 3.) On the leader, as soon as we respond to the hardLock request, we block MED and LOW lanes and simulate high load
      // 4.) Then we wait until the shards are back into sync

      const col = db._collection(collectionName);
      const shardName = col.shards()[0];
      // Insert some random documents, to avoid getting away with a fast-path sync
      col.save([{}, {}, {}, {}]);

      for (const server of getDBServers()) {
        debugSetFailAt(getEndpointById(server.id), `LeaderWrongChecksumOnSoftLock${shardName}`);
        debugSetFailAt(getEndpointById(server.id), `FollowerBlockRequestsLanesForSyncOnShard${shardName}`);
        debugSetFailAt(getEndpointById(server.id), `LeaderBlockRequestsLanesForSyncOnShard${shardName}`);
      }

      /* Increase replication factor, this will trigger the failurePointCascade */
      col.properties({replicationFactor: 2});
      // This should now wait until the collection get back into sync using only HIGH lanes
      waitForShardsInSync(collectionName, 20, 1);
      // Unlock medium lanes again. The collections are synced
      // so we can revert back to normal state.
      clearAllFailurePoints();
      try {
        // Should be able to insert documents again
        col.save({});
      } catch (e) {
        fail();
      }
    }
  };
}

if (debugCanUseFailAt()) {
  jsunity.run(ReplicationDeadLockSuite);
}

return jsunity.done();
