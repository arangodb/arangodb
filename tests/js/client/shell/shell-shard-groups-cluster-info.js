////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////
/* global arango, assertTrue, assertEqual, assertNotEqual */
"use strict";

const jsunity = require("jsunity");
const {db} = require("@arangodb");

const database = "ClusterInfoTests";

function ClusterInfoShardGroupsTest() {

  function getClusterInfo() {
    return arango.GET("_api/cluster/cluster-info");
  }

  return {
    setUp: function () {
      db._createDatabase(database);
      db._useDatabase(database);
    },
    tearDown: function () {
      db._useDatabase("_system");
      db._dropDatabase(database);
    },

    testGetClusterInfo: function () {
      assertNotEqual(getClusterInfo(), undefined);
    },

    testCollectionWithMultipleShards : function() {
      const c = db._create("C", {numberOfShards: 3});

      const shards = c.shards();
      const ci = getClusterInfo();
      for (const s of shards) {
        assertEqual(ci.shardGroups[s], undefined);
        assertEqual(ci.shardToShardGroupLeader[s], undefined);
      }
    },

    testDistributeShardsLikeCollectionWithMultipleShards : function() {
      const c = db._create("C", {numberOfShards: 3});
      const d = db._create("D", {numberOfShards: 3, distributeShardsLike: "C"});

      const shardsC = c.shards();
      const shardsD = d.shards();
      assertEqual(shardsC.length, shardsD.length);
      {
        const ci = getClusterInfo();
        for (let k = 0; k < shardsC.length; k++) {
          assertNotEqual(ci.shardGroups[shardsC[k]].indexOf(shardsD[k]), -1);
          assertEqual(ci.shardToShardGroupLeader[shardsD[k]], shardsC[k]);
        }
      }

      d.drop();
      {
        const ci = getClusterInfo();
        for (let k = 0; k < shardsC.length; k++) {
          assertEqual(ci.shardGroups[shardsC[k]].indexOf(shardsD[k]), -1);
          assertEqual(ci.shardToShardGroupLeader[shardsD[k]], undefined);
        }
      }
    },

  };
}

jsunity.run(ClusterInfoShardGroupsTest);
return jsunity.done();
