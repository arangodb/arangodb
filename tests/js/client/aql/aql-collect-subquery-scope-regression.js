/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, fail */

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

const jsunity = require("jsunity");
const internal = require("internal");
const errors = internal.errors;
const db = require("@arangodb").db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function subqueryCollectScopeSuite() {
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test regression with incorrect scoping of collect variable
///        inside subquery
////////////////////////////////////////////////////////////////////////////////

    testSubqueryCollectScopeOk : function () {
      const query = `LET t = (COLLECT foo = undefined RETURN true) RETURN true`;
      try {
        db._query(query);
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
      }
    },

    testSubqueryCollectScopeOk2 : function () {
      const query = `LET t = (COLLECT foo = 5 RETURN true) RETURN true`;
      let actual = db._query(query);
      assertEqual(actual.toArray(), [ true ]);
    },

    testCollectUsingItsOwnVariablesInIntoExpression : function () {
      const queries = [
        "FOR x IN [] COLLECT c = x INTO g = c RETURN g",
        "FOR x IN [] COLLECT c = x INTO g = 1 + c RETURN g",
        "FOR x IN [] COLLECT c = x INTO g = (RETURN c) RETURN g",
        "FOR x IN [] COLLECT c = x INTO g = (FOR j IN [] RETURN c) RETURN g",
        "FOR x IN [] COLLECT c = x INTO g = (FOR j IN [] FOR k IN [] RETURN c) RETURN g",
        "FOR x IN [] COLLECT c = x INTO g = (LET a = c RETURN a) RETURN g",
        "FOR x IN [] COLLECT c = x INTO g = (LET a = c LET b = a RETURN b) RETURN g",
        "FOR x IN [] COLLECT c = x INTO g = (FOR j IN [] RETURN (FOR k IN [] RETURN c + 1)) RETURN g",
        "FOR x IN [] COLLECT c = x, d = x + 1 INTO g = d RETURN g",
        "FOR x IN [] COLLECT AGGREGATE c = LENGTH(x) INTO g = c RETURN g",
        "FOR x IN [] COLLECT AGGREGATE c = LENGTH(x) INTO g = (FOR j IN [] RETURN (FOR k IN [] RETURN c + 1)) RETURN g",
      ];

      queries.forEach((query) => {
        try {
          db._query(query);
          fail();
        } catch (e) {
          assertEqual(e.errorNum, errors.ERROR_QUERY_VARIABLE_NAME_UNKNOWN.code, query);
        }
      });
    },

    // Regression: group assignments must not reference other group outputs from
    // the same COLLECT. Those outputs only exist after CollectNode; evaluating
    // them earlier produced an invalid plan and MissingVariablesException.
    testCollectUsingItsOwnVariablesInGroupExpression : function () {
      const queries = [
        "FOR x IN [] COLLECT a = 1, b = a RETURN b",
        "FOR x IN [] COLLECT a = 1, b = [a] RETURN b",
        "FOR x IN [] COLLECT a = 1, b = 2, c = [a, b] RETURN c",
        "FOR x IN [] COLLECT a = 1, b = (RETURN a) RETURN b",
        "FOR x IN [] COLLECT a = 1, b = (FOR j IN [] RETURN a) RETURN b",
        `FOR br IN [
           { name: "Magic Hat", abv: 5.0 },
           { name: "Midnight Sun Brewing Co.", abv: 6.5 }
         ]
         COLLECT
           br_name = br.name,
           nestedVar = 1,
           nestedVar_1 = 2,
           nestedVar_2 = 3,
           a_1_2_3_ = [nestedVar, nestedVar_1, nestedVar_2]
         AGGREGATE max = MAX(br.abv)
         RETURN { br_name, a_1_2_3_, max }`,
      ];

      queries.forEach((query) => {
        try {
          db._query(query);
          fail();
        } catch (e) {
          assertEqual(e.errorNum, errors.ERROR_QUERY_VARIABLE_NAME_UNKNOWN.code, query);
        }
      });
    },

  };
}

jsunity.run(subqueryCollectScopeSuite);

return jsunity.done();
