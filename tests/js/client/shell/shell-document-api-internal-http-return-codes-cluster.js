/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global assertEqual, arango */

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
const arangodb = require('@arangodb');

const reconnectRetry = require('@arangodb/replication-common').reconnectRetry;
const primaryEndpoint = arango.getEndpoint();
let { getEndpointById } = require('@arangodb/test-helper');

function leaderTestSuite () {
  'use strict';
  const cn = "UnitTestsCollection";
  let shardName;
  
  return {
    setUpAll: function () {
      db._drop(cn);
      // We create only one shard so we have exactly 1 leader and 1 follower.
      const c = db._create(cn, { numberOfShards: 1, replicationFactor: 2 });
      const shards = c.shards(true);
      // We create exactly one shard
      assertEqual(Object.entries(shards).length, 1);
      for (const [shardId, servers] of Object.entries(shards)) {
        // Pick the leader of the first shard, and connect to it
        let leader = getEndpointById(servers[0]);
        reconnectRetry(leader, "_system", "root", "");

        // Memorize the shardName for network connection
        shardName = shardId
        // Stop the loop, we will only have one anyways, however it is clear that we no talk to a leader
        break;
      }
      // Just to be sure we have a shard now.
      assertTrue(shardName.length > 0);
    },

    tearDownAll: function () {
      // Reconnect back to original server
      reconnectRetry(primaryEndpoint, "_system", "root", "");
      db._drop(cn);
    },

    // In all tests our arango is connected to the leader of our collection.

    testDummy: function () {
      const result = arango.GET_RAW("/_api/document/" + encodeURIComponent(shardName) + "/test");
      assertEqual(404, result.code);
    },
  };
}

function followerTestSuite () {
  'use strict';
  const cn = "UnitTestsCollection";
  let shardName;
  
  return {
    setUpAll: function () {
      db._drop(cn);
      // We create only one shard so we have exactly 1 leader and 1 follower.
      const c = db._create(cn, { numberOfShards: 1, replicationFactor: 2 });
      const shards = c.shards(true);
      // We create exactly one shard
      assertEqual(Object.entries(shards).length, 1);
      for (const [shardId, servers] of Object.entries(shards)) {
        // Pick the leader of the first shard, and connect to it
        let follower = getEndpointById(servers[1]);
        reconnectRetry(follower, "_system", "root", "");

        // Memorize the shardName for network connection
        shardName = shardId
        // Stop the loop, we will only have one anyways, however it is clear that we no talk to a follower
        break;
      }
      // Just to be sure we have a shard now.
      assertTrue(shardName.length > 0);
    },

    tearDownAll: function () {
      // Reconnect back to original server
      reconnectRetry(primaryEndpoint, "_system", "root", "");
      db._drop(cn);
    },

    // In all tests our arango is connected to the follower of our collection.

    testDummy2: function () {
      const result = arango.GET_RAW("/_api/document/" + encodeURIComponent(shardName) + "/test");
      assertEqual(421, result.code);
    },
  };
}



jsunity.run(leaderTestSuite);
jsunity.run(followerTestSuite);
return jsunity.done();
