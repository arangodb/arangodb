/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, fail */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
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

// Keep this equal to kMaxDeterminizeSeconds in
// lib/iresearch/core/utils/wildcard_utils.cpp.
const kMaxDeterminizeSeconds = 5.0;

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const db = arangodb.db;
const internal = require("internal");
const errors = internal.errors;

function wildcardDeterminizationBudgetSuite() {
  const dbName = "WildcardDeterminizationBudget";
  let pattern = "%a" + "_".repeat(20);
  const query = `FOR d IN v SEARCH LIKE(d.name, "${pattern}") OPTIONS { waitForSync: true } RETURN d`;

  // Allowance for the time check, which runs between determinization steps,
  // and for work done before that loop starts.
  const slack = 1.0;

  function expectedBudget(maxRuntime) {
    if (maxRuntime > 0) {
      return Math.min(kMaxDeterminizeSeconds, maxRuntime);
    }
    return kMaxDeterminizeSeconds;
  }

  function assertBudget(maxRuntime) {
    const budget = expectedBudget(maxRuntime);
    const options = maxRuntime > 0 ? {'maxRuntime': maxRuntime} : {};
    const started = internal.time();
    try {
      db._query(query, {}, options).toArray();
      fail("pathological wildcard should have been rejected, maxRuntime=" +
           maxRuntime);
    } catch (e) {
      const elapsed = internal.time() - started;

      let expectedErrorStrings = ["time budget", "query killed"];
      let errMsgFound = (expectedErrorStrings.some(s => e.errorMessage.includes(s)));

      assertTrue(errMsgFound, e.errorMessage);
      assertTrue(elapsed >= budget,
                 `elapsed ${elapsed}s, expected at least ${budget}s ` +
                 `(maxRuntime=${maxRuntime})`);
      assertTrue(elapsed < budget + slack,
                 `elapsed ${elapsed}s, expected less than ${budget + slack}s ` +
                 `(maxRuntime=${maxRuntime})`);
    }
  }

  return {
    setUpAll: function () {
      db._useDatabase("_system");
      try {
        db._dropDatabase(dbName);
      } catch (err) {}
      db._createDatabase(dbName);
      db._useDatabase(dbName);
      let coll = db._create("c");
      db._createView("v", "arangosearch", {
        "links": {"c": {"includeAllFields": true}}
      });
      coll.insert({ name: 'John' });
      db._query("for d in v OPTIONS { waitForSync: true } return d").toArray();
    },

    tearDownAll: function () {
      db._useDatabase("_system");
      try {
        db._dropDatabase(dbName);
      } catch (err) {}
    },

    testHarmlessWildcardStillRuns: function () {
      // A short wildcard determinizes within time budget threshold.
      const result = db._query(
          "FOR d IN v SEARCH LIKE(d.name, 'a_c%') RETURN d.name").toArray();
      assertEqual([], result);
    },

    testBudgetUsesMaxRuntimeWhenItIsSmaller: function () {
      // min(kMaxDeterminizeSeconds, 2) === 2
      assertBudget(2);
    },

    testBudgetCapsAtDeterminizeLimitWhenMaxRuntimeIsLarger: function () {
      // min(kMaxDeterminizeSeconds, 30) === kMaxDeterminizeSeconds
      assertBudget(30);
    },

    testBudgetUsesDeterminizeLimitWhenMaxRuntimeIsUnset: function () {
      assertBudget(0);
    }
  };
}

jsunity.run(wildcardDeterminizationBudgetSuite);

return jsunity.done();
