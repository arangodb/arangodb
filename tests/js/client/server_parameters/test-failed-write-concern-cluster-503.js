/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertNotEqual, assertTrue, arango */

////////////////////////////////////////////////////////////////////////////////
/// @brief test for configurable behaviour of failed write concern
///
/// DISCLAIMER
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
/// Copyright holder is ArangoDB Inc, Cologne, Germany
///
/// @author Max Neunhoeffer
/// @author Copyright 2023, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'cluster.failed-write-concern-error-code': '503'
  };
}
let jsunity = require('jsunity');
const errors = require('@arangodb').errors;
const cn = "UnitTestsCollection";
let db = require('internal').db;
let reconnectRetry = require('@arangodb/replication-common').reconnectRetry;

let { getEndpointById
    } = require('@arangodb/test-helper');

function testSuite() {
  return {
    setUp: function() {
      db._drop(cn);
      db._create(cn, { numberOfShards: 1, replicationFactor: 2, writeConcern: 2 });
    },
    
    tearDown: function() {
      db._drop(cn);
    },
    
    testFailedBehaviour : function() {
      let coordinator = arango.getEndpoint();

      let c = db._collection(cn);
      let shards = c.shards(true);
      let shard = Object.keys(shards)[0];
      let servers = shards[shard];
      assertEqual(2, servers.length);
      let follower1 = getEndpointById(servers[1]);

      try {
        reconnectRetry(follower1, "_system", "root", "");
        arango.PUT_RAW("/_admin/debug/failat/LogicalCollection::insert", {});
        arango.PUT_RAW("/_admin/debug/failat/SynchronizeShard::disable", {});
        reconnectRetry(coordinator, "_system", "root", "");
        arango.PUT_RAW("/_admin/debug/failat/CoordinatorInsert::TimeoutLow", {});

        c.insert({Hallo:1});  // This drops the followers, but works
        let startTime = new Date();
        let res = arango.POST_RAW(`/_api/document/${cn}`, {Hallo:2});
        assertEqual(503, res.code);
        let timeSpentMs = new Date() - startTime;
        // The failure point CoordinatorInsert::TimeoutLow we use sets the
        // timeout to 10 seconds (as opposed to the default 900 seconds).
        // This means that this call usually returns after about 9 seconds
        // due to the exponential backoff we are using. Therefore the
        // tolerance of 2 to 30 seconds should be good enough to make the
        // test stable.
        assertTrue(timeSpentMs > 2000 && timeSpentMs < 30000);
      } finally {
        reconnectRetry(follower1, "_system", "root", "");
        arango.DELETE_RAW("/_admin/debug/failat/LogicalCollection::insert");
        arango.DELETE_RAW("/_admin/debug/failat/SynchronizeShard::disable");
        reconnectRetry(coordinator, "_system", "root", "");
        arango.DELETE_RAW("/_admin/debug/failat/CoordinatorInsert::TimeoutLow");
      }
    },
    
  };
}

jsunity.run(testSuite);
return jsunity.done();
