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
const console = require('console');
let jsunity = require('jsunity');
let internal = require('internal');
let arangodb = require('@arangodb');
let fs = require('fs');
let db = arangodb.db;
let { waitForShardsInSync } = require('@arangodb/test-helper');
let { instanceRole } = require('@arangodb/testutils/instance');
let IM = global.instanceManager;

function queryAgencyJob (id) {
  let query = arango.GET(`/_admin/cluster/queryAgencyJob?id=${id}`);
  if (query.status === "Finished") {
    return {error: false, id, status: query.status};
  }
  return {error: true, errorMsg: "Did not find job.", id};
}

function getEndpointAndIdMap() {
  const health = arango.GET("/_admin/cluster/health").Health;
  const endpointMap = {};
  const idMap = {};
  for (let sid in health) {
    endpointMap[health[sid].ShortName] = health[sid].Endpoint;
    idMap[health[sid].ShortName] = sid;
  }
  return {endpointMap, idMap};
}

function createCollectionWithKnownLeaderAndFollower(cn) {
  db._create(cn, {numberOfShards:1, replicationFactor:2});
  // Get dbserver names first:
  let { endpointMap, idMap } = getEndpointAndIdMap();
  let plan = arango.GET("/_admin/cluster/shardDistribution").results[cn].Plan;
  let shard = Object.keys(plan)[0];
  let coordinator = "Coordinator0001";
  let leader = plan[shard].leader;
  let follower = plan[shard].followers[0];
  return { endpointMap, idMap, coordinator, leader, follower, shard };
}

function moveShard(database, collection, shard, fromServer, toServer, dontwait) {
  let body = {database, collection, shard, fromServer, toServer};
  let result;
  try {
    result = arango.POST_RAW("/_admin/cluster/moveShard", body);
  } catch (err) {
    console.error(
      "Exception for PUT /_admin/cluster/moveShard:", err.stack);
    return false;
  }
  if (dontwait) {
    return result;
  }
  // Now wait until the job we triggered is finished:
  let count = 600;   // seconds
  while (true) {
    let job = queryAgencyJob(result.parsedBody.id);
    if (job.error === false && job.status === "Finished") {
      return result;
    }
    if (count-- < 0) {
      console.error(
        "Timeout in waiting for moveShard to complete: "
        + JSON.stringify(body));
      return false;
    }
    require("internal").wait(1.0);
  }
}

function moveShardSynchronizeShardFailureSuite() {
  'use strict';
  const cn = 'UnitTestsMoveShardSyncShardFailure';

  const setupTeardown = function () {
    db._drop(cn);
  };

  return {
    setUp: setupTeardown,
    tearDown: setupTeardown,
    
    testMoveShardSynchronizeShardFailure: function() {
      let collInfo = createCollectionWithKnownLeaderAndFollower(cn);
      // We have a shard whose leader and follower is known to us.
      
      let followerEndpoint = collInfo.endpointMap[collInfo.follower];
      let leaderEndpoint = collInfo.endpointMap[collInfo.leader];
      let followerId = collInfo.idMap[collInfo.follower];
      let slpPath = global.instanceManager.rootDir;
      // C++ will find this directory via ARANGOTEST_ROOT_DIR from the
      // environment.
      let progPath = fs.join(slpPath, "globalSLP");
      let pcPath = fs.join(slpPath, "globalSLP_PC");

      try {
        // Let's insert some documents:
        let c = db._collection(cn);
        for (let i = 0; i < 100; ++i) {
          c.insert({Hallo:i});
        }

        // Deploy a straight line program to stage events:
        // Note that `collInfo.leader` is the **old** leader and
        // `collInfo.follower` is the **old** follower, whose roles 
        // are supposed to swap during the moveShard!
        // We want that first the old leader discovers a SynchronizeShard,
        // and that then the new leader takes over shard leadership,
        // tells its followers and reports to Current, and that **only then**
        // the SynchrnoizeShard job is executed on the old leader (now 
        // follower) and then fails.
        let prog = [
          `SynchronizeShard::beginning ${collInfo.leader}:${collInfo.shard} SynchronizeShardStarted`,
          `HandleLeadership::before ${collInfo.follower}:${collInfo.shard} HandleLeadershipBefore`,
          `HandleLeadership::after ${collInfo.follower}:${collInfo.shard} HandleLeadershipAfter`,
          `Maintenance::BeforePhaseTwo ${collInfo.follower} LeaderSendsCurrent`,
          `ClusterInfo::loadCurrentSeesLeader ${collInfo.leader}:${collInfo.shard}:${followerId} FollowerUpdatesCurrent`,
          `ClusterInfo::loadCurrentDone ${collInfo.leader} FollowerHasUpdatedCurrent`,
          `SynchronizeShard::beginning2 ${collInfo.leader}:${collInfo.shard} SynchronizeShardStartedContinuing`,
          `SynchronizeShard::beforeSetTheLeader ${collInfo.leader}:${collInfo.shard} SynchronizeShardRunning`,
          `SynchronizeShard::beginning ${collInfo.leader}:${collInfo.shard} SynchronizeShardStarted2`,
          `SynchronizeShard::beginning2 ${collInfo.leader}:${collInfo.shard} SynchronizeShardStartedContinuing2`,
          `SynchronizeShard::beforeSetTheLeader ${collInfo.leader}:${collInfo.shard} SynchronizeShardRunning2`,
          ""  // want to have a new line at the end of the last line!
        ];
        fs.writeFileSync(progPath, prog.join("\n"));
        fs.writeFileSync(pcPath, "");

        // Activate failure points in leader and follower:
        IM.debugSetFailAt("HandleLeadership::before", '', followerEndpoint);
        IM.debugSetFailAt("HandleLeadership::after", '', followerEndpoint);
        IM.debugSetFailAt("Maintenance::BeforePhaseTwo", '', followerEndpoint);
        IM.debugSetFailAt("SynchronizeShard::beginning", '', leaderEndpoint);
        IM.debugSetFailAt("SynchronizeShard::beforeSetTheLeader", '', leaderEndpoint);
        IM.debugSetFailAt("ClusterInfo::loadCurrentSeesLeader", '', leaderEndpoint);
        IM.debugSetFailAt("ClusterInfo::loadCurrentDone", '', leaderEndpoint);
        IM.debugSetFailAt("SynchronizeShard::fail", '', leaderEndpoint);

        // Now move the shard:
        let res = moveShard("_system", cn, collInfo.shard, collInfo.leader, collInfo.follower, false /* dontwait */);

        // See if the program was executed:
        let count = 0;
        let nrSteps;
        while (count <= 600) {
          let steps = fs.readFileSync(pcPath).toString().split(/\r\n|\r|\n/g);
          // Take care of some silly Windows magic which messes with my 
          // line ends.
          nrSteps = steps.length - 1;
          if (nrSteps === prog.length - 1) {
            break;
          }
          count += 1;
          internal.wait(1);
        }
        assertEqual(nrSteps, prog.length - 1);
      } finally {
        // Remove program and failure points:
        fs.remove(progPath);
        fs.remove(pcPath);
        IM.debugClearFailAt();
      }
    },
    
  };
}

jsunity.run(moveShardSynchronizeShardFailureSuite);
return jsunity.done();
