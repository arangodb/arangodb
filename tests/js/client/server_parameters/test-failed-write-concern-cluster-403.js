/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertNotEqual, assertTrue, arango */

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
// //////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'cluster.failed-write-concern-status-code': '403'
  };
}
let jsunity = require('jsunity');
const errors = require('@arangodb').errors;
const cn = "UnitTestsCollection";
let db = require('internal').db;
let lh = require('@arangodb/testutils/replicated-logs-helper');
let lpreds = require('@arangodb/testutils/replicated-logs-predicates');
let reconnectRetry = require('@arangodb/replication-common').reconnectRetry;
let IM = global.instanceManager;

const replicationVersion = db._properties().replicationVersion;

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
      let c = db._collection(cn);
      let shards = c.shards(true);
      let shard = Object.keys(shards)[0];
      let servers = shards[shard];
      assertEqual(2, servers.length);

      let follower1 = IM.getInstanceByID(servers[1]);

      try {
        // Insert two documents:
        let d1 = c.insert({Hallo:1});
        let d2 = c.insert({Hallo:2});

        if (replicationVersion === "1") {
          follower1.debugSetFailAt("LogicalCollection::insert");
          follower1.debugSetFailAt("SynchronizeShard::disable");
          let d = c.insert({Hallo:3});  // This drops the followers, but works
        } else {
          // stop the follower, causing a failed write concern
          follower1.suspend();
          lh.waitFor(lpreds.serverFailed(follower1.id));
        }

        // INSERT test, single document:
        let startTime = new Date();
        let res = arango.POST_RAW(`/_api/document/${cn}`, {Hallo:2});
        assertEqual(403, res.code);
        let timeSpentMs = new Date() - startTime;
        // With this error code the coordinator does not do a retry. Therefore,
        // we should not get a significant delay. The 5 seconds are enough to
        // keep the test stable (usually, the call returns after
        // 3 milliseconds).
        assertTrue(timeSpentMs < 5000);

        // INSERT test, batch:
        startTime = new Date();
        res = arango.POST_RAW(`/_api/document/${cn}`, [{Hallo:2},{Hallo:3}]);
        assertEqual(403, res.code);
        timeSpentMs = new Date() - startTime;
        // With this error code the coordinator does not do a retry. Therefore,
        // we should not get a significant delay. The 5 seconds are enough to
        // keep the test stable (usually, the call returns after
        // 3 milliseconds).
        assertTrue(timeSpentMs < 5000);

        // REPLACE test, single document:
        startTime = new Date();
        res = arango.PUT_RAW(`/_api/document/${cn}/d._key`, {Hallo:2});
        assertEqual(403, res.code);
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
        assertEqual(403, res.code);
        timeSpentMs = new Date() - startTime;
        // With this error code the coordinator does not do a retry. Therefore,
        // we should not get a significant delay. The 5 seconds are enough to
        // keep the test stable (usually, the call returns after
        // 3 milliseconds).
        assertTrue(timeSpentMs < 5000);

        // UPDATE test, single document:
        startTime = new Date();
        res = arango.PATCH_RAW(`/_api/document/${cn}/d._key`, {Hallo:2});
        assertEqual(403, res.code);
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
        assertEqual(403, res.code);
        timeSpentMs = new Date() - startTime;
        // With this error code the coordinator does not do a retry. Therefore,
        // we should not get a significant delay. The 5 seconds are enough to
        // keep the test stable (usually, the call returns after
        // 3 milliseconds).
        assertTrue(timeSpentMs < 5000);

        // DELETE test, single document:
        startTime = new Date();
        res = arango.DELETE_RAW(`/_api/document/${cn}/d._key`, {});
        assertEqual(403, res.code);
        timeSpentMs = new Date() - startTime;
        // With this error code the coordinator does not do a retry. Therefore,
        // we should not get a significant delay. The 5 seconds are enough to
        // keep the test stable (usually, the call returns after
        // 3 milliseconds).
        assertTrue(timeSpentMs < 5000);

        // DELETE test, batch:
        startTime = new Date();
        res = arango.DELETE_RAW(`/_api/document/${cn}`, [d1._key, d2._key]);
        assertEqual(403, res.code);
        timeSpentMs = new Date() - startTime;
        // With this error code the coordinator does not do a retry. Therefore,
        // we should not get a significant delay. The 5 seconds are enough to
        // keep the test stable (usually, the call returns after
        // 3 milliseconds).
        assertTrue(timeSpentMs < 5000);

      } finally {
        if (replicationVersion === "1") {
          follower1.debugClearFailAt("LogicalCollection::insert");
          follower1.debugClearFailAt("SynchronizeShard::disable");
        } else {
          follower1.resume();
          lh.waitFor(lpreds.serverHealthy(follower1.id));
        }
      }
    },
  };
}

jsunity.run(testSuite);
return jsunity.done();
