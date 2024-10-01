/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertEqual, assertTrue, assertFalse, arango, getOptions */

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
// of threads in the scheduler. 60 threads means that there are 30 low
// priority lane threads. We will post 200 background jobs which will
// start an exclusive transaction and thus get stuck in a lock.
// This totally blocks all scheduler threads. If the detaching
// of scheduler threads waiting for this lock works, then we can Unblock
// this situation pretty soon (30 threads per second). So after a few
// seconds, a normal low priority operation will go through again.
// If you use fewer than 51 threads, then the cluster overwhelm protections
// will also block the situation since the coordinator will no longer
// dequeue low priority lane jobs. If you use more than 200 threads, then
// the test will not test the detaching of threads.
if (getOptions === true) {
  return {
    'server.maximal-threads' : '60',
    'server.minimal-threads' : '60'
  };
}

const _ = require('lodash');
const console = require('console');
let jsunity = require('jsunity');
let internal = require('internal');
let arangodb = require('@arangodb');
let fs = require('fs');
let db = arangodb.db;

function detachSchedulerThreadsSuite() {
  'use strict';
  const cn = 'UnitTestsDetachSchedulerThreads';
  const cn2 = 'UnitTestsDetachSchedulerThreads2';

  const setupTeardown = function () {
    db._drop(cn);
    db._drop(cn2);
  };

  return {
    setUp: setupTeardown,
    tearDown: setupTeardown,
    
    testStartManyExclusiveTrxs: function() {
      let c = db._create(cn);
      let c2 = db._create(cn2);

      // Let's insert some documents:
      let l = [];
      for (let i = 0; i < 100; ++i) {
        l.push({Hallo:i});
      }
      c.insert(l);
      c2.insert(l);

      // Create a transaction with an exclusive lock and write something:
      let t = db._createTransaction({collections:{exclusive:[cn]}});
      let cc = t.collection(cn);
      cc.insert({});

      // Now create 200 more writes all waiting for the lock, but
      // asynchronously, without detached threads, this will make all
      // low prio threads block:
      let ts = [];
      for (let i = 0; i < 200; ++i) {
        ts.push(arango.POST_RAW(`/_api/document/${cn}`,{Write: i},
                {"x-arango-async": "store"})
          .headers["x-arango-async-id"]);

      }
      // If threads detach, we should first be able to write to the second
      // collection without too much delay:
      c2.insert({"CanProceed": true});

      // And then we can commit the transaction:
      t.commit();

      // Clean up jobs:
      for (let id of ts) {
        let count = 0;
        while (true) {
          count += 1;
          if (count >= 60) {
            throw "Jobs did not succeeed in time!";
          }
          let r = arango.PUT_RAW(`/_api/job/${id}`, {});
          if (r.code === 202) {
            break;
          }
          internal.wait(0.5);
        }
      }

    },
    
  };
}

jsunity.run(detachSchedulerThreadsSuite);
return jsunity.done();
