/*jshint globalstrict:false, strict:false */
/*global global.ArangoClusterInfo, assertEqual, assertNotEqual, assertTrue, fail, arango, assertMatch */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

let jsunity = require("jsunity");
let arangodb = require("@arangodb");
let internal = require("internal");
let db = arangodb.db;
const {
  getResponsibleServersFromClusterInfo
} = require('@arangodb/test-helper');

function ResponsibleServersSuite() {
  const cn = "UnitTestsCollection";

  return {

    setUp : function () {
      db._drop(cn + "1");
      db._drop(cn + "2");
    },

    tearDown : function () {
      db._drop(cn + "1");
      db._drop(cn + "2");
    },
    
    testCheckNonExistingCollection : function () {
      [ "xxxxx", "s-1234", "fuppe!", "S12345", "" ].forEach(function(arg) {
        let res = getResponsibleServersFromClusterInfo([arg]);
        assertMatch(/no shard found with ID/, res);
      });
    },
    
    testCheckBrokenParameters : function () {
      let c = db._create(cn + "1", { numberOfShards: 5 });
        
      [ null, false, true, 123, "foo", [] ].forEach(function(arg) {
        let res = getResponsibleServersFromClusterInfo([arg]);
        assertMatch(/no shard found with ID/, res);

      });
    },

    testCheckWithSingleShard : function () {
      let c = db._create(cn + "1", { numberOfShards: 1 });

      let expected, actual;
      let tries = 0;
      while (++tries < 3) {
        let dist = c.shards(true);
        let shards = Object.keys(dist);
        assertNotEqual(0, shards.length);
        expected = dist[shards[0]][0];

        let servers = getResponsibleServersFromClusterInfo([shards[0]]);

        assertTrue(servers.hasOwnProperty(shards[0]));

        actual = servers[shards[0]];
        // the retrieval of collection.shards() and global.ArangoClusterInfo.getResponsibleServers()
        // is not atomic. there may be a leader change in between
        if (expected === actual) {
          // no leader change
          break;
        }

        // supposedly a leader change. try again
        internal.wait(1, false);
      }

      assertEqual(expected, actual);
    },
    
    testCheckWithMultipleShards : function () {
      let c = db._create(cn + "1", { numberOfShards: 5 });

      let expected, actual;
      let tries = 0;
      while (++tries < 3) {
        let dist = c.shards(true);
        let shards = Object.keys(dist);
        assertEqual(5, shards.length);
        expected = {};
        shards.forEach(function(shard) {
          expected[shard] = dist[shard][0];
        });
        actual = getResponsibleServersFromClusterInfo(shards);
        
        assertEqual(5, Object.keys(actual).length);

        // the retrieval of collection.shards() and global.ArangoClusterInfo.getResponsibleServers()
        // is not atomic. there may be a leader change in between
        try {
          // this is actually a hack, we are using assertEqual here to compare the two values
          // if they are equal, this will not fail. if unequal, this will throw an exception
          assertEqual(expected, actual);
          // no leader change
          break;
        } catch (err) {
          // unequal
        }

        // supposedly a leader change. try again
        internal.wait(1, false);
      }

      assertEqual(expected, actual);
    },
    
    testCheckWithMultipleCollections : function () {
      let c1 = db._create(cn + "1", { numberOfShards: 5 });
      let c2 = db._create(cn + "2", { numberOfShards: 5 });

      let expected, actual;
      let tries = 0;
      while (++tries < 3) {
        let dist = c1.shards(true);
        let shards = Object.keys(dist);
        assertEqual(5, shards.length);
        let dist2 = c2.shards(true);
        shards = shards.concat(Object.keys(dist2));
        assertEqual(10, shards.length);
        Object.keys(dist2).forEach(function(d) {
          dist[d] = dist2[d];
        });

        expected = {};
        shards.forEach(function(shard) {
          expected[shard] = dist[shard][0];
        });
        actual = getResponsibleServersFromClusterInfo(shards);

        assertEqual(10, Object.keys(actual).length);

        // the retrieval of collection.shards() and global.ArangoClusterInfo.getResponsibleServers()
        // is not atomic. there may be a leader change in between
        try {
          // this is actually a hack, we are using assertEqual here to compare the two values
          // if they are equal, this will not fail. if unequal, this will throw an exception
          assertEqual(expected, actual);
          // no leader change
          break;
        } catch (err) {
          // unequal
        }

        // supposedly a leader change. try again
        internal.wait(1, false);
      }

      assertEqual(expected, actual);
    },
    
  };
}

jsunity.run(ResponsibleServersSuite);

return jsunity.done();
