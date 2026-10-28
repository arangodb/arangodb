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
// //////////////////////////////////////////////////////////////////////////////

let jsunity = require('jsunity');
let arangodb = require('@arangodb');
let db = arangodb.db;
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
