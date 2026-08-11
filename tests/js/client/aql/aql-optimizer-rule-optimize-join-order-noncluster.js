/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertNotEqual */

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
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;

function optimizeJoinOrderTestSuite () {
  const ruleName = "optimize-join-order";

  const c1 = "UnitTestsOptimizeJoinOrder1";
  const c2 = "UnitTestsOptimizeJoinOrder2";

  // The rule is disabled by default, so it must be requested explicitly.
  const paramForced   = { optimizer: { rules: [ "-all", "+" + ruleName ] } };
  const paramBase     = { optimizer: { rules: [ "-all" ] } };
  const paramDefault  = { };

  function rules(options, query) {
    return db._createStatement({ query, bindVars: {}, options }).explain().plan.rules;
  }

  return {

    setUpAll : function () {
      db._drop(c1);
      db._drop(c2);
      let col1 = db._create(c1);
      let col2 = db._create(c2);
      let docs1 = [], docs2 = [];
      for (let i = 0; i < 10; ++i) {
        docs1.push({ value: i });
        docs2.push({ value: i });
      }
      col1.insert(docs1);
      col2.insert(docs2);
    },

    tearDownAll : function () {
      db._drop(c1);
      db._drop(c2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief rule is disabled by default and must not appear unless requested
////////////////////////////////////////////////////////////////////////////////

    testDisabledByDefault : function () {
      const query = `FOR a IN ${c1} FOR b IN ${c2} FILTER a.value == b.value RETURN a.value`;
      // with the full default optimizer the rule must NOT be applied
      assertEqual(-1, rules(paramDefault, query).indexOf(ruleName), query);
      // and with -all it is not applied either
      assertEqual(-1, rules(paramBase, query).indexOf(ruleName), query);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief rule fires (is reported as applied) when there is an actual equijoin
////////////////////////////////////////////////////////////////////////////////

    testRuleHasEffect : function () {
      const queries = [
        `FOR a IN ${c1} FOR b IN ${c2} FILTER a.value == b.value RETURN a.value`,
        `FOR a IN ${c1} FOR b IN ${c2} FILTER a.value == b.value FOR c IN ${c1} FILTER b.value == c.value RETURN a.value`,
        // disconnected join graph (two independent equijoins) still fires
        `FOR a IN ${c1} FOR b IN ${c2} FILTER a.value == b.value FOR c IN ${c1} FOR d IN ${c2} FILTER c.value == d.value RETURN a.value`,
      ];
      queries.forEach(function (query) {
        assertNotEqual(-1, rules(paramForced, query).indexOf(ruleName), query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief rule does NOT fire when there is no equijoin to build a graph from
////////////////////////////////////////////////////////////////////////////////

    testRuleNoEffect : function () {
      const queries = [
        // single enumeration, no join
        `FOR a IN ${c1} FILTER a.value == 1 RETURN a.value`,
        // cross product without an equijoin condition
        `FOR a IN ${c1} FOR b IN ${c2} RETURN a.value`,
        // join against a list, not two collections
        `FOR a IN ${c1} FOR b IN 1..10 FILTER a.value == b RETURN a.value`,
      ];
      queries.forEach(function (query) {
        assertEqual(-1, rules(paramForced, query).indexOf(ruleName), query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief the rule must not change query results (it is a no-op on the plan)
////////////////////////////////////////////////////////////////////////////////

    testResultsInvariant : function () {
      const queries = [
        `FOR a IN ${c1} FOR b IN ${c2} FILTER a.value == b.value RETURN [a.value, b.value]`,
        `FOR a IN ${c1} FOR b IN ${c2} FILTER a.value == b.value FILTER a.value > 3 RETURN a.value`,
        `FOR a IN ${c1} FOR b IN ${c2} FILTER a.value == b.value FOR c IN ${c1} FILTER b.value == c.value RETURN a.value`,
      ];
      queries.forEach(function (query) {
        // -all vs -all + rule => plans are identical (rule is a no-op), so the
        // result including ordering must be byte-identical.
        const base   = db._query(query, {}, paramBase).toArray();
        const forced = db._query(query, {}, paramForced).toArray();
        assertEqual(base, forced, query);
        // and under the full default optimizer results are unchanged too
        // (compare as multisets, since full optimization may reorder rows)
        const norm = (arr) => arr.map((x) => JSON.stringify(x)).sort();
        const dflt = db._query(query, {}, paramDefault).toArray();
        assertEqual(norm(base), norm(dflt), query);
      });
    },

  };
}

jsunity.run(optimizeJoinOrderTestSuite);

return jsunity.done();
