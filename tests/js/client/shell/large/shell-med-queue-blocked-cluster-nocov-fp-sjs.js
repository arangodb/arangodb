/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertEqual, assertTrue, assertFalse, arango */

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

let jsunity = require('jsunity');
let arangodb = require('@arangodb');
let db = arangodb.db;
let { getEndpointsByType } = require('@arangodb/test-helper');
let { instanceRole } = require('@arangodb/testutils/instance');
let IM = global.instanceManager;

function medQueueBlockedSuite() {
  'use strict';
  const cn = 'UnitTestsMedQueueBlocked';

  return {

    setUp: function () {
      db._drop(cn);
    },

    tearDown: function () {
      db._drop(cn);
    },
    
    testRunAql: function() {
      let coll = db._create(cn, {numberOfShards:3});
      let l = [];
      for (let i = 0; i < 100; ++i) {
        l.push({"Hello":i});
      }
      coll.insert(l);

      // We now want to overload the medium prio scheduler queue, to this
      // end, we post some perverse queries asynchronously. This should
      // overwhelm any server without the fix. Note that we post the query
      // with x-arango-frontend: true, such that we can start enough queries
      // concurrently to fill the medium queue on the coordinator.
      const f = require("@arangodb/aql/functions");
      try {
        f.register("USER::FUNC", function() {
          const db = require("@arangodb").db;
          require("internal").wait(1);
          db._query("FOR d IN @@c RETURN d", {"@c": "UnitTestsMedQueueBlocked"});
          return 12;
        });
        let start = new Date();
        let jobs = []; 
        for (let i = 0; i < 100; ++i) {
          jobs.push(arango.POST_RAW("/_api/cursor",
            {query:`LET s = USER::FUNC() FOR d IN ${cn} RETURN {d,s}`},
            {"x-arango-async": "store", "x-arango-frontend": true}).headers["x-arango-async-id"]);
        }
        for (let j of jobs) {
          while (true) {
            let res = arango.PUT_RAW(`/_api/job/${j}`, {});
            if (res.code !== 204) {
              break;
            }
            if (new Date() - start > 50000) {
              throw "Lost patience, almost certainly the coordinator is deadlocked!";
            }
            require("internal").wait(0.1);
          }
        }
        let end = new Date();
        // Why 50 seconds? If only two of the queries are executed concurrently,
        // then this should be done in 50s. Usually, there will be a higher parallelism
        // and it should finish much faster. Without the bug fixed, it will lock up and
        // not finish within 50s.
        assertTrue(end - start < 50000);   // Should be done in 50 seconds
      } finally {
        f.unregister("USER::FUNC");
        coll.drop();
      }
    },

    testCreateColl: function() {
      try {
        // This failure point blocks the medium priority queue. This means
        // that the response to an AgencyCache poll operation to the AgencyCache
        // can only work if it skips the scheduler. That means that a collection
        // creation can only succeed if this is the case. Note that we send
        // our collection creation request with the header "x-arango-frontend"
        // set to true, land it on the high prio queue:
        arango.PUT_RAW("/_admin/debug/failat/BlockSchedulerMediumQueue", {});
        // We are using the direct connection to set the failure point here,
        // since for this particular failure point (block scheduler medium
        // queue) a reconnect is not possible. Therefore we cannot use
        // debugFailAt.

        let start = new Date();
        let savedTimeout = arango.timeout();
        arango.timeout(20);
        let res = arango.POST_RAW("/_api/collection", {"name":cn}, {"x-arango-frontend": true});
        let end = new Date();
        arango.timeout(savedTimeout);
        assertTrue(end - start < 10000);   // Should be done in 10 seconds
      } finally {
        // We are using the direct connection to set the failure point here,
        // since for this particular failure point (block scheduler medium
        // queue) a reconnect is not possible. Therefore we cannot use
        // debugFailAt.
        arango.DELETE_RAW("/_admin/debug/failat");
        require("internal").wait(3);  // give the system time to finish the 
                                      // creation of the collection, in case
                                      // the test TestCreateColl fails.
      }
    },

  };
}

jsunity.run(medQueueBlockedSuite);
return jsunity.done();
