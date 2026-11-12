/*jshint globalstrict:false, strict:false, maxlen: 400 */
/*global fail, assertEqual, assertTrue */

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

function newMonitorMemoryLimitSuite() {
  'use strict';
  // memoryLimit drives only the new monitor (temporary, see Query.cpp).
  const query = "FOR i IN 1..100000 RETURN CONCAT('foobarbaz', i)";

  return {
    testQueryOverNewMonitorLimitFailsWithResourceLimit: function () {
      // Without the limit the same query succeeds.
      assertEqual(100000, db._query(query, null, {memoryLimit: 0}).toArray().length);

      try {
        db._query(query, null, {memoryLimit: 64 * 1024}).toArray();
        fail();
      } catch (err) {
        assertEqual(internal.errors.ERROR_RESOURCE_LIMIT.code, err.errorNum, err.errorMessage);
      }
    },

    testLimitSweepFailsCleanlyUntilQuerySucceeds: function () {
      // Deterministic: no collection, no randomness.
      const sweepQuery = "FOR i IN 1..10000 RETURN CONCAT('foobarbaz', i)";
      const expected = db._query(sweepQuery, null, {memoryLimit: 0}).toArray();
      assertEqual(10000, expected.length);

      // Each step lets the query get further, so the limit hits a different allocation site.
      const step = 4 * 1024;
      const maxLimit = 8 * 1024 * 1024;
      let failures = 0;
      let result = null;
      for (let limit = step; limit <= maxLimit && result === null; limit += step) {
        let error = null;
        try {
          result = db._query(sweepQuery, null, {memoryLimit: limit}).toArray();
        } catch (err) {
          error = err;
        }
        if (error !== null) {
          assertEqual(internal.errors.ERROR_RESOURCE_LIMIT.code, error.errorNum,
                      `limit ${limit}: ${error.errorMessage}`);
          ++failures;
        }
      }
      assertTrue(failures > 0, "a 4 KB limit should already fail");
      assertEqual(expected, result, "no limit up to 8 MB was enough");
    },
  };
}

jsunity.run(newMonitorMemoryLimitSuite);

return jsunity.done();
