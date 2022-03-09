/*jshint globalstrict:false, strict:false */
/*global fail, assertFalse, assertTrue, assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief test add/drop followers
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
let db = require("@arangodb").db;
let internal = require("internal");

function FollowersSuite () {
  'use strict';

  const cn = "UnitTestsCollection";

  return {
    setUp : function () {
      internal.debugClearFailAt();
      db._drop(cn);
    },
    
    tearDown : function () {
      internal.debugClearFailAt();
      db._drop(cn);
    },
    
    testNoReplication : function () {
      let c = db._create(cn, { numberOfShards: 5, replicationFactor: 1 });

      let result = require("@arangodb/cluster").shardDistribution().results[cn];

      // validate Plan
      assertTrue(result.hasOwnProperty("Plan"));
      let shards = Object.keys(result.Plan);
      shards.forEach(function(shard) {
        let data = result.Plan[shard];
        assertTrue(data.hasOwnProperty("leader"));
        assertTrue(data.hasOwnProperty("followers"));
        // supposed to have a no follower
        assertEqual(0, data.followers.length);
      });

      // now check Current
      assertTrue(result.hasOwnProperty("Current"));

      shards = Object.keys(result.Current);
      assertEqual(5, shards.length);
      shards.forEach(function(shard) {
        let data = result.Current[shard];
        assertTrue(data.hasOwnProperty("leader"));
        assertTrue(data.hasOwnProperty("followers"));
        assertEqual(0, data.followers.length);
      });
    },

    testWithReplication : function () {
      let c = db._create(cn, { numberOfShards: 5, replicationFactor: 2 });

      let result = require("@arangodb/cluster").shardDistribution().results[cn];

      // validate Plan
      assertTrue(result.hasOwnProperty("Plan"));
      let shards = Object.keys(result.Plan);
      assertEqual(5, shards.length);
      shards.forEach(function(shard) {
        let data = result.Plan[shard];
        assertTrue(data.hasOwnProperty("leader"));
        assertTrue(data.hasOwnProperty("followers"));
        // supposed to have a single follower
        assertEqual(1, data.followers.length);
        // follower must be != leader
        assertEqual(-1, data.followers.indexOf(data.leader));
      });

      // now check Current
      assertTrue(result.hasOwnProperty("Current"));

      let tries = 0;
      let found = 0;
      while (++tries < 60) {
        found = 0;
        shards = Object.keys(result.Current);
        assertEqual(5, shards.length);
        shards.forEach(function(shard) {
          let data = result.Current[shard];
          assertTrue(data.hasOwnProperty("leader"));
          assertTrue(data.hasOwnProperty("followers"));
          // supposed to have a single follower
          if (data.followers.length !== 1) {
            return;
          }
          assertEqual(1, data.followers.length);
          // follower must be != leader
          assertEqual(-1, data.followers.indexOf(data.leader));
          ++found;
        });

        if (found === shards.length) {
          break;
        }

        internal.wait(0.5, false);
        result = require("@arangodb/cluster").shardDistribution().results[cn];
      }

      assertEqual(shards.length, found);
    },

    testWithReplicationAndFailure : function () {
      if (!internal.debugCanUseFailAt()) {
        return;
      }

      internal.debugSetFailAt("FollowerInfo::add");

      // Note that with waitForSyncReplication: true, the collection creation
      // will run into a timeout while waiting for the followers to come in
      // sync.
      db._create(cn, { numberOfShards: 5, replicationFactor: 2 }, { waitForSyncReplication: false });

      let result = require("@arangodb/cluster").shardDistribution().results[cn];

      // validate Plan
      assertTrue(result.hasOwnProperty("Plan"));
      let shards = Object.keys(result.Plan);
      assertEqual(5, shards.length);
      shards.forEach(function(shard) {
        let data = result.Plan[shard];
        assertTrue(data.hasOwnProperty("leader"));
        assertTrue(data.hasOwnProperty("followers"));
        // supposed to have a single follower
        assertEqual(1, data.followers.length);
        // follower must be != leader
        assertEqual(-1, data.followers.indexOf(data.leader));
      });

      // now check Current
      assertTrue(result.hasOwnProperty("Current"));

      let tries = 0;
      // try for 10 seconds, and in this period no followers must show up 
      while (++tries < 20) {
        shards = Object.keys(result.Current);
        assertEqual(5, shards.length);
        shards.forEach(function(shard) {
          let data = result.Current[shard];
          assertTrue(data.hasOwnProperty("leader"));
          assertTrue(data.hasOwnProperty("followers"));
          // supposed to have a no followers
          assertEqual(0, data.followers.length);
        });

        internal.wait(0.5, false);
        result = require("@arangodb/cluster").shardDistribution().results[cn];
      }
    }

  };
}

jsunity.run(FollowersSuite);

return jsunity.done();
