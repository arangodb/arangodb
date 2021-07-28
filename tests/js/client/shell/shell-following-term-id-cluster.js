/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertEqual, assertTrue, assertFalse, arango */

// //////////////////////////////////////////////////////////////////////////////
// / @brief Test following term id behaviour.
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

const _ = require('lodash');
let jsunity = require('jsunity');
let internal = require('internal');
let arangodb = require('@arangodb');
let db = arangodb.db;

function createCollectionWithKnownLeaderAndFollower(cn) {
  db._create(cn, {numberOfShards:1, replicationFactor:2});
  // Get dbserver names first:
  let health = arango.GET("/_admin/cluster/health").Health;
  let endpointMap = {};
  let idMap = {};
  for (let sid in health) {
    endpointMap[health[sid].ShortName] = health[sid].Endpoint;
    idMap[health[sid].ShortName] = sid;
  }
  let plan = arango.GET("/_admin/cluster/shardDistribution").results[cn].Plan;
  let shard = Object.keys(plan)[0];
  let coordinator = "Coordinator0001";
  let leader = plan[shard].leader;
  let follower = plan[shard].followers[0];
  return { endpointMap, idMap, coordinator, leader, follower, shard };
}

function switchConnectionToCoordinator(collInfo) {
  arango.reconnect(collInfo.endpointMap[collInfo.coordinator], "_system", "root", "");
}

function switchConnectionToLeader(collInfo) {
  arango.reconnect(collInfo.endpointMap[collInfo.leader], "_system", "root", "");
}

function switchConnectionToFollower(collInfo) {
  arango.reconnect(collInfo.endpointMap[collInfo.follower], "_system", "root", "");
}

function followingTermIdSuite() {
  'use strict';
  const cn = 'UnitTestsFollowingTermId';
  let collInfo = {};

  return {

    setUp: function () {
      db._drop(cn);
      collInfo = createCollectionWithKnownLeaderAndFollower(cn);
    },

    tearDown: function () {
      db._drop(cn);
    },
    
    testFollowingTermIdSuite: function() {
      // We have a shard whose leader and follower is known to us.
      
      // Let's insert some documents:
      let c = db._collection(cn);
      for (let i = 0; i < 100; ++i) {
        c.insert({Hallo:i});
      }

      // Now check that both leader and follower have 100 documents:
      switchConnectionToLeader(collInfo);
      assertEqual(100, db._collection(collInfo.shard).count());
      switchConnectionToFollower(collInfo);
      assertEqual(100, db._collection(collInfo.shard).count());

      // Try to insert a document with the leaderId:
      let res = arango.POST(`/_api/document/${collInfo.shard}?isSynchronousReplication=${collInfo.idMap[collInfo.leader]}`, {Hallo:101});
      assertTrue(res.error);
      assertEqual(406, res.code);
      assertEqual(1490, res.errorNum);

      switchConnectionToCoordinator(collInfo);
      
      // Now insert another document:
      c.insert({Hallo:101});

      // And check that both leader and follower have 101 documents:
      switchConnectionToLeader(collInfo);
      assertEqual(101, db._collection(collInfo.shard).count());
      switchConnectionToFollower(collInfo);
      assertEqual(101, db._collection(collInfo.shard).count());

      switchConnectionToCoordinator(collInfo);
    },
    
  };
}

jsunity.run(followingTermIdSuite);
return jsunity.done();
