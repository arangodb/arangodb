/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, getOptions, assertEqual, assertTrue, assertFalse, arango */

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

// We need the following options for this test. We need a specific number 
// of threads in the scheduler. 30 threads means that there are 15 low
// priority lane threads. We will post 100 background jobs which will
// at first block on `transaction::Manager::leaseManagedTrx` because of
// a lock. This totally blocks all scheduler threads. If the detaching
// of scheduler threads waiting for this lock works, then we can Unblock
// this situation pretty soon (15 threads per second). So after a few
// seconds, a normal low priority operation will go through again.
// If you use fewer than 25 threads, then the cluster overwhelm protections
// will also block the situation since the coordinator will no longer
// dequeue low priority lane jobs. If you use more than 200 threads, then
// the test will not test the detaching of threads.
// We need to set the timeout for acquiring the lease on a managed trx
// to 600, since its default is 8 seconds which would ruin the test.

if (getOptions === true) {
  return {
    'transaction.streaming-lock-timeout' : '600',
    'transaction.streaming-idle-timeout' : '120',
    'server.maximal-threads' : '30',
    'server.minimal-threads' : '30'
  };
}

const _ = require('lodash');
let jsunity = require('jsunity');
let internal = require('internal');
let arangodb = require('@arangodb');
let fs = require('fs');
let db = arangodb.db;
let IM = global.instanceManager;

function createCollectionWithKnownLeaderAndFollower(cn) {
  db._create(cn, {numberOfShards:1, replicationFactor:2});
  // Get dbserver names first:
  let plan = arango.GET("/_admin/cluster/shardDistribution").results[cn].Plan;
  let shard = Object.keys(plan)[0];
  return {
    coordinator: "Coordinator0001",
    leader: plan[shard].leader,
    follower: plan[shard].followers[0],
    shard: shard
  };
}

function detachSchedulerThreadsSuite2() {
  'use strict';
  const cn = 'UnitTestsDetachSchedulerThreads';

  const setupTeardown = function () {
    db._drop(cn);
  };

  return {
    setUp: setupTeardown,
    tearDown: setupTeardown,
    
    testDelayReplicationInExclusiveTrx: function() {
      // We want to create a lockup in leaseManagedTrx.

      let collInfo = createCollectionWithKnownLeaderAndFollower(cn);
      // We have a shard whose leader and follower is known to us.
      


      // Let's insert some documents:
      let c = db._collection(cn);
      let l = [];
      for (let i = 0; i < 100; ++i) {
        l.push({_key:"K"+i, Hallo:i});
      }
      c.insert(l);

      try {
        // Block replication in the follower, such that it can be released
        // later on (nested failure points).
        IM.debugSetFailAt("synchronousReplication::blockReplication", '', collInfo.follower);

        // Create a transaction T writing to the replicated collection
        let trx = arango.POST_RAW("/_api/transaction/begin", {collections:{write:[cn]}});
        assertEqual(201, trx.code);
        trx = trx.parsedBody.result.id;

        // Perform one insert within T in the background, it will block, since
        // it cannot replicate.
        let blocked = arango.POST_RAW(`/_api/document/${cn}`, {Hallo:1}, {"x-arango-async": "store", "x-arango-trx-id": trx});
        assertEqual(202, blocked.code);
        assertTrue(blocked.headers.hasOwnProperty("x-arango-async-id"));
        blocked = blocked.headers["x-arango-async-id"];

        // Perform multiple read operations within T, they should fight over
        // the lease of the transaction and block many threads of the same prio.
        let l = [blocked];
        for (let i = 0; i < 100; ++i) {
          let r = arango.POST_RAW(`/_api/document/${cn}`, {_key:"L"+i}, {"x-arango-async": "store", "x-arango-trx-id": trx});
          assertEqual(202, r.code);
          assertTrue(r.headers.hasOwnProperty("x-arango-async-id"));
          l.push(r.headers["x-arango-async-id"]);
        }

        // Then perform simple reads to the collection. They should soon be
        // unblocked, because of detached threads.
        c.document("K1");

        // Unblock everything:
        IM.debugSetFailAt("synchronousReplication::unblockReplication", '', collInfo.follower);

        // And now expect that all ops terminate:
        let waitForJob = function(id) {
          let count = 0;
          while(true) {
            let r = arango.PUT_RAW(`/_api/job/${id}`, "");
            if (r.code !== 204) {
              assertTrue(!r.parsedBody.hasOwnProperty("error"));
              return r;
            }
            if (++count >= 600) {
              throw "Did not finish in time to wait for jobs.";
            }
            internal.wait(0.5);
          }
        };
        for (let j of l) {
          waitForJob(j);
        }

        // And abort transaction:
        let res = arango.DELETE_RAW(`/_api/transaction/${trx}`);
        assertEqual(200, res.code);
      } finally {
        IM.debugClearFailAt('', '', collInfo.follower);
      }
    }
  };
}

if (db._properties().replicationVersion !== "2" &&
    IM.debugCanUseFailAt()) {
  jsunity.run(detachSchedulerThreadsSuite2);
}
return jsunity.done();
