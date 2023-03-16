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
    'cluster.failed-write-concern-status-code': '503'
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
        // Insert two documents:
        let d1 = c.insert({Hallo:1});
        let d2 = c.insert({Hallo:2});

        reconnectRetry(follower1, "_system", "root", "");
        arango.PUT_RAW("/_admin/debug/failat/LogicalCollection::insert", {});
        arango.PUT_RAW("/_admin/debug/failat/SynchronizeShard::disable", {});
        reconnectRetry(coordinator, "_system", "root", "");

        let d = c.insert({Hallo:1});  // This drops the followers, but works

        // INSERT test, single document:
        let startTime = new Date();
        let res = arango.POST_RAW(`/_api/document/${cn}`, {Hallo:2});
        assertEqual(503, res.code);
        let timeSpentMs = new Date() - startTime;
        // With this error code the coordinator does not do a retry
        // either. Therefore, we should not get a significant delay. The
        // 5 seconds are enough to keep the test stable (usually, the
        // call returns after 3 milliseconds).
        assertTrue(timeSpentMs < 5000);

        // INSERT test, batch:
        startTime = new Date();
        res = arango.POST_RAW(`/_api/document/${cn}`, [{Hallo:2},{Hallo:3}]);
        assertEqual(503, res.code);
        timeSpentMs = new Date() - startTime;
        // With this error code the coordinator does not do a retry. Therefore,
        // we should not get a significant delay. The 5 seconds are enough to
        // keep the test stable (usually, the call returns after
        // 3 milliseconds).
        assertTrue(timeSpentMs < 5000);

        // REPLACE test, single document:
        startTime = new Date();
        res = arango.PUT_RAW(`/_api/document/${cn}/d._key`, {Hallo:2});
        assertEqual(503, res.code);
        timeSpentMs = new Date() - startTime;
        // With this error code the coordinator does not do a retry. Therefore,
        // we should not get a significant delay. The 5 seconds are enough to
        // keep the test stable (usually, the call returns after
        // 3 milliseconds).
        assertTrue(timeSpentMs < 5000);

        // REPLACE test, batch:
        startTime = new Date();
        res = arango.PUT_RAW(`/_api/document/${cn}`,
             [{_key:d1._key, Hallo:2},{_key:d2._key, Hallo:3}]);
        assertEqual(503, res.code);
        timeSpentMs = new Date() - startTime;
        // With this error code the coordinator does not do a retry. Therefore,
        // we should not get a significant delay. The 5 seconds are enough to
        // keep the test stable (usually, the call returns after
        // 3 milliseconds).
        assertTrue(timeSpentMs < 5000);

        // UPDATE test, single document:
        startTime = new Date();
        res = arango.PATCH_RAW(`/_api/document/${cn}/d._key`, {Hallo:2});
        assertEqual(503, res.code);
        timeSpentMs = new Date() - startTime;
        // With this error code the coordinator does not do a retry. Therefore,
        // we should not get a significant delay. The 5 seconds are enough to
        // keep the test stable (usually, the call returns after
        // 3 milliseconds).
        assertTrue(timeSpentMs < 5000);

        // UPDATE test, batch:
        startTime = new Date();
        res = arango.PATCH_RAW(`/_api/document/${cn}`,
             [{_key:d1._key, Hallo:2},{_key:d2._key, Hallo:3}]);
        assertEqual(503, res.code);
        timeSpentMs = new Date() - startTime;
        // With this error code the coordinator does not do a retry. Therefore,
        // we should not get a significant delay. The 5 seconds are enough to
        // keep the test stable (usually, the call returns after
        // 3 milliseconds).
        assertTrue(timeSpentMs < 5000);

        // DELETE test, single document:
        startTime = new Date();
        res = arango.DELETE_RAW(`/_api/document/${cn}/d._key`, {});
        assertEqual(503, res.code);
        timeSpentMs = new Date() - startTime;
        // With this error code the coordinator does not do a retry. Therefore,
        // we should not get a significant delay. The 5 seconds are enough to
        // keep the test stable (usually, the call returns after
        // 3 milliseconds).
        assertTrue(timeSpentMs < 5000);

        // DELETE test, batch:
        startTime = new Date();
        res = arango.DELETE_RAW(`/_api/document/${cn}`, [d1._key, d2._key]);
        assertEqual(503, res.code);
        timeSpentMs = new Date() - startTime;
        // With this error code the coordinator does not do a retry. Therefore,
        // we should not get a significant delay. The 5 seconds are enough to
        // keep the test stable (usually, the call returns after
        // 3 milliseconds).
        assertTrue(timeSpentMs < 5000);


      } finally {
        reconnectRetry(follower1, "_system", "root", "");
        arango.DELETE_RAW("/_admin/debug/failat/LogicalCollection::insert");
        arango.DELETE_RAW("/_admin/debug/failat/SynchronizeShard::disable");
        reconnectRetry(coordinator, "_system", "root", "");
      }
    },
    
  };
}

jsunity.run(testSuite);
return jsunity.done();
