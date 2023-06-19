/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertEqual, assertTrue, assertFalse, arango */

// //////////////////////////////////////////////////////////////////////////////
// / @brief timeouts during query setup
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

let jsunity = require('jsunity');
let arangodb = require('@arangodb');
let db = arangodb.db;
let { getEndpointsByType,
      debugCanUseFailAt,
      debugClearFailAt,
      debugSetFailAt,
    } = require('@arangodb/test-helper');
      
function medQueueBlockedSuite() {
  'use strict';
  const cn = 'UnitTestsMedQueueBlocked';

  return {

    setUp: function () {
      // We are using the direct connection to set the failure point here,
      // since for this particular failure point (block scheduler medium
      // queue) a reconnect is not possible. Therefore we cannot use
      // debugFailAt.
      arango.DELETE_RAW("/_admin/debug/failat");
      db._drop(cn);
    },

    tearDown: function () {
      // We are using the direct connection to set the failure point here,
      // since for this particular failure point (block scheduler medium
      // queue) a reconnect is not possible. Therefore we cannot use
      // debugFailAt.
      arango.DELETE_RAW("/_admin/debug/failat");
      db._drop(cn);
    },
    
    testCreateColl: function() {
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
    },

    testRunAql: function() {
      // This failure point blocks the medium priority queue, when the query
      // is finished, but before the cleanup has been done. This tests that
      // this cleanup works if the medium prio queue is full.
      arango.PUT_RAW("/_admin/debug/failat/BlockSchedulerMediumQueueBeforeCleanupAQL", {});
      // We are using the direct connection to set the failure point here,
      // since for this particular failure point (block scheduler medium
      // queue) a reconnect is not possible. Therefore we cannot use
      // debugFailAt.

      let start = new Date();
      let savedTimeout = arango.timeout();
      arango.timeout(20);
      let res = arango.POST_RAW("/_api/cursor", {query:"FOR d IN c RETURN d", collections: { write: [cn] } }, {"x-arango-frontend": true});
      let end = new Date();
      arango.timeout(savedTimeout);
      assertTrue(end - start < 10000);   // Should be done in 10 seconds
    },
  };
}

let ep = getEndpointsByType('coordinator');
if (ep.length && debugCanUseFailAt(ep[0])) {
  // only execute if failure tests are available
  jsunity.run(medQueueBlockedSuite);
}
return jsunity.done();
