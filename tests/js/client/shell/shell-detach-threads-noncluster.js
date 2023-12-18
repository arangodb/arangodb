/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertEqual, assertTrue, assertFalse, arango */

// //////////////////////////////////////////////////////////////////////////////
// / @brief Test detaching threads in scheduler which wait for locks.
// /
// / DISCLAIMER
// /
// / Copyright 2023 ArangoDB GmbH, Cologne, Germany
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
// / @author Max Neunhoeffer
// //////////////////////////////////////////////////////////////////////////////

const _ = require('lodash');
const console = require('console');
let jsunity = require('jsunity');
let internal = require('internal');
let arangodb = require('@arangodb');
let fs = require('fs');
let db = arangodb.db;
let { debugCanUseFailAt, debugRemoveFailAt, debugSetFailAt, debugClearFailAt } = require('@arangodb/test-helper');

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
