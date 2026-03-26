/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse, fail */

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
// //////////////////////////////////////////////////////////////////////////////

const internal = require("internal");
const jsunity = require("jsunity");
const db = internal.db;
const helper = require("@arangodb/aql-helper");
const test_helper = require("@arangodb/test-helper");
const assertQueryError = helper.assertQueryError;
const errors = internal.errors;
const cn = "UnitTestsCollection";
const vn = "UnitTestView";
const optimizerRuleName = "immutable-search-condition";

function assertRuleFires(query, param) {
  const result = test_helper.AQL_EXPLAIN(query, param);
  assertTrue(result.plan.rules.includes(optimizerRuleName),
             `[${result.plan.rules}] does not contain "${optimizerRuleName}"`);
}

function assertRuleDoesNotFire(query, param) {
  const result = test_helper.AQL_EXPLAIN(query, param);
  assertFalse(result.plan.rules.includes(optimizerRuleName),
             `[${result.plan.rules}] contains "${optimizerRuleName}"`);
}

function findLoopSuite () {
  return {
    testIResearchImmutableSearchCondition : function() {
      db._create(cn);
      const v = db._createView(vn, 'arangosearch');
      v.properties({ links: { [cn]: { includeAllFields: true }}});

      db._query(`FOR i IN 1..1 INSERT { yearly: 2025 } IN @@col`, { "@col": cn });
      db._query(`FOR i IN 1..2 INSERT { yearly: 2024 } IN @@col`, { "@col": cn });
      db._query("FOR d IN @@view OPTIONS { waitForSync: true } RETURN d", { "@view": vn });

      const q = `FOR year IN 2024..2025
                   LET startOfYear = year
                   LET endOfYear = year+1
                   LET yearlyData = (
                   FOR q IN @@view
                     SEARCH q.yearly >= startOfYear AND q.yearly < endOfYear
                     RETURN 1)
                   RETURN {
                     year,
                     dataCount: LENGTH(yearlyData),
                 }`;

      assertRuleDoesNotFire(q, { "@view": vn });
      const r = db._query(q, { "@view": vn });

      assertEqual(r.toArray(),
           [ { "year" : 2024, 
               "dataCount" : 2 
             }, 
             { 
               "year" : 2025, 
               "dataCount" : 1 
             } ]);
      db._dropView(vn);
      db._drop(cn);

      },
    testIResearchImmutableSearchCondition2 : function() {
      db._create(cn);
      const v = db._createView(vn, 'arangosearch');
      v.properties({ links: { [cn]: { includeAllFields: true }}});

      db._query(`FOR i IN 1..1 INSERT { yearly: 2025 } IN @@col`, { "@col": cn });
      db._query(`FOR i IN 1..2 INSERT { yearly: 2024 } IN @@col`, { "@col": cn });
      db._query("FOR d IN @@view OPTIONS { waitForSync: true } RETURN d", { "@view": vn });

      const q = `FOR year IN 2024..2025
                   LET startOfYear = 2024
                   LET endOfYear = 2025
                   LET yearlyData = (
                   FOR q IN @@view
                     SEARCH q.yearly >= startOfYear AND q.yearly < endOfYear
                     RETURN 1)
                   RETURN {
                     year,
                     dataCount: LENGTH(yearlyData),
                   }`;
      assertRuleFires(q, { "@view": vn });
      db._dropView(vn);
      db._drop(cn);
      },

   };
} 

jsunity.run(findLoopSuite);
return jsunity.done();
