/*jshint globalstrict:false, strict:false, maxlen: 400 */
/*global fail, assertEqual */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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

// Needs an arangod built with USE_MEMORY_TRACKING: without the malloc
// override the new monitor counts nothing and its limit never fires.

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const internal = require("internal");
const IM = global.instanceManager;

function newMonitorMemoryLimitSuite() {
  'use strict';
  // Allocates far more than the 64 KB set by the failure point. The old
  // monitor's limit stays off (memoryLimit: 0), so only the new one can fire.
  const query = "FOR i IN 1..100000 RETURN CONCAT('foobarbaz', i)";

  return {
    setUp: function () {
      IM.debugClearFailAt();
    },

    tearDown: function () {
      IM.debugClearFailAt();
    },

    testQueryOverNewMonitorLimitFailsWithResourceLimit: function () {
      // Without the limit the same query succeeds.
      assertEqual(100000, db._query(query, null, {memoryLimit: 0}).toArray().length);

      IM.debugSetFailAt("Query::smallNewMonitorLimit");
      try {
        db._query(query, null, {memoryLimit: 0}).toArray();
        fail();
      } catch (err) {
        assertEqual(internal.errors.ERROR_RESOURCE_LIMIT.code, err.errorNum, err.errorMessage);
      }
    },
  };
}

jsunity.run(newMonitorMemoryLimitSuite);

return jsunity.done();
