/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertTrue, assertFalse, assertEqual,
   assertNotEqual, arango, print */

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
// / @author Michael Hackstein
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const {db} = require('@arangodb');
let { instanceRole } = require('@arangodb/testutils/instance');
let IM = global.instanceManager;

const {
  getDBServers,
  waitForShardsInSync
} = require('@arangodb/test-helper');

function ReplicationDeadLockSuite() {
  const collectionName = "UnitTestCollection";

  return {
    setUp: function () {
      // On purpose we use replicationFactor 1 here.
      // We need to increase it in the test
      db._create(collectionName, {replicationFactor: 1});
    },

    tearDown: function () {
      IM.debugClearFailAt('', instanceRole.dbServer);
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

      IM.debugSetFailAt(`LeaderWrongChecksumOnSoftLock${shardName}`, instanceRole.dbServer);
      IM.debugSetFailAt(`FollowerBlockRequestsLanesForSyncOnShard${shardName}`, instanceRole.dbServer);
      IM.debugSetFailAt(`LeaderBlockRequestsLanesForSyncOnShard${shardName}`, instanceRole.dbServer);

      /* Increase replication factor, this will trigger the failurePointCascade */
      col.properties({replicationFactor: 2});
      // This should now wait until the collection get back into sync using only HIGH lanes
      waitForShardsInSync(collectionName, 20, 1);
      // Unlock medium lanes again. The collections are synced
      // so we can revert back to normal state.
      print('clearing');
      IM.debugClearFailAt('', instanceRole.dbServer);
      try {
        // Should be able to insert documents again
        col.save({});
      } catch (e) {
        fail();
      }
    }
  };
}

jsunity.run(ReplicationDeadLockSuite);
return jsunity.done();
