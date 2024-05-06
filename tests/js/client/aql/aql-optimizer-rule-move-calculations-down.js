/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual */

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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const helper = require("@arangodb/aql-helper");
const db = require("@arangodb").db;
const isCluster = require("internal").isCluster();

function optimizerRuleTestSuite () {
  const ruleName = "move-calculations-down";

  // various choices to control the optimizer: 
  const paramNone   = { optimizer: { rules: [ "-all" ] } };
  const paramEnabled    = { optimizer: { rules: [ "-all", "+" + ruleName ] } };
  const paramDisabled  = { optimizer: { rules: [ "+all", "-" + ruleName ] } };

  const RulesCombinator = function*(listOfRules) {
    const RuleOnOff = function* (rule) {
      yield `-${rule}`;
      yield `+${rule}`;
    };
    const RecursiveRuleOnOff = function* (list) {
      if (list.length === 0) {
        return;
      }
      const rule = list[0];
      for (const r of RuleOnOff(rule)) {
        if (list.length === 1) {
          yield r;
        } else {
          for (const other of RecursiveRuleOnOff(list.slice(1))) {
            yield `${r} ${other}`;
          }
        }
      }
    };
    listOfRules.push("all");
    for (const rules of RecursiveRuleOnOff(listOfRules)) {
      yield {optimizer: {rules} };
    }
  };

  const cn = "UnitTestsAhuacatlCalculation";
  let c;

  return {
    setUpAll: function () {
      db._drop(cn);
      c = db._create(cn);
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i, value: i });
      }
      c.insert(docs);
    },

    tearDownAll: function () {
      db._drop(cn);
      c = null;
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test that rule has no effect when explicitly disabled
    ////////////////////////////////////////////////////////////////////////////////

    testRuleDisabled: function () {
      const queries = [
        "FOR i IN 1..10 LET result = i + 1 FILTER i < 5 RETURN result",
        "FOR i IN 1..10 LET a = i + 1 LET b = i + 2 FILTER i < 2 LET c = b FILTER i < 10 RETURN c",
        "FOR i IN 1..10 LET result = IS_STRING(i) FILTER i < 2 RETURN result",
        "FOR i IN 1..10 LET result = IS_STRING(i) LIMIT 1 RETURN result",
        "FOR i IN 1..10 LET result = IS_STRING(i) SORT i RETURN result",
        "FOR i IN 1..10 LET result = PUSH(i, i + 1) LET x = (FOR j IN result RETURN j) RETURN x",
        "FOR i IN 1..10 LET result = (RETURN i + 1) FILTER i < 5 RETURN result",
        "FOR i IN 1..10 LET a = (RETURN i + 1) LET b = (RETURN i + 2) FILTER i < 2 LET c = b FILTER i < 10 RETURN c",
        "FOR i IN 1..10 LET result = (RETURN IS_STRING(i)) FILTER i < 2 RETURN result",
        "FOR i IN 1..10 LET result = (RETURN IS_STRING(i)) LIMIT 1 RETURN result",
        "FOR i IN 1..10 LET result = (RETURN IS_STRING(i)) SORT i RETURN result",
        "FOR i IN 1..10 LET result = (RETURN PUSH(i, i + 1)) LET x = (FOR j IN result RETURN j) RETURN x",
      ];

      queries.forEach(function (query) {
        let result = db._createStatement({query, bindVars: {}, options: paramNone}).explain();
        assertEqual([], result.plan.rules.filter((r) => r !== "splice-subqueries"), query);
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test that rule has no effect
    ////////////////////////////////////////////////////////////////////////////////

    testRuleNoEffect: function () {
      const queries = [
        "LET a = PASSTHRU(39) FOR i IN 1..10 RETURN a",
        "FOR i IN 1..10 RETURN i",
        "FOR i IN 1..10 FILTER i < 10 RETURN i",
        "FOR i IN 1..10 FILTER i < 10 FILTER i + 1 < 10 RETURN i",
        "FOR i IN 1..10 FILTER i < 10 FILTER i + 1 < 10 LIMIT 2 RETURN i",
        "FOR i IN 1..10 LET result = i < 5 FILTER result RETURN i",
        "FOR i IN 1..10 LET result = i + 1 SORT result RETURN i",
        "FOR i IN 1..10 LET result = NOOPT(i + 1) LET test = (FOR j IN 1..result RETURN j) RETURN test",
        "FOR i IN 1..10 LIMIT 2 LET result = i < 5 RETURN i",
        "FOR i IN 1..10 LET result = IS_STRING(i) FOR j IN 1..2 RETURN j",
        "FOR i IN 1..10 FILTER i < 10 LET result = IS_STRING(i) FOR j IN 1..2 RETURN j",
        "FOR i IN 1..10 LET result = IS_STRING(i) COLLECT r = result RETURN r",
        "FOR i IN 1..10 LET result = MAX(i) FILTER result < 3 RETURN result",
        "FOR i IN 1..10 LET result = RAND() FILTER i < 10 RETURN result",
        "LET a = (RETURN PASSTHRU(39)) FOR i IN 1..10 RETURN a",
        "FOR i IN 1..10 LET result = (RETURN i < 5) FILTER result RETURN i",
        "FOR i IN 1..10 LET result = (RETURN i + 1) SORT result RETURN i",
        "FOR i IN 1..10 LET result = NOOPT(RETURN i + 1) LET test = (FOR j IN 1..NOOPT(result) RETURN j) RETURN test",
        "FOR i IN 1..10 LIMIT 2 LET result = (RETURN i < 5) RETURN i",
        "FOR i IN 1..10 LET result = (RETURN IS_STRING(i)) FOR j IN 1..2 RETURN j",
        "FOR i IN 1..10 FILTER i < 10 LET result = (RETURN IS_STRING(i)) FOR j IN 1..2 RETURN j",
        "FOR i IN 1..10 LET result = (RETURN IS_STRING(i)) COLLECT r = result RETURN r",
        "FOR i IN 1..10 LET result = (RETURN MAX(i)) FILTER result < 3 RETURN result",
        "FOR i IN 1..10 LET result = (RETURN RAND()) FILTER i < 10 RETURN result",
        // 2 uncorrelated subqueries:
        "FOR i IN 1..10 LET result = (RETURN i + 1) LET test = (FOR j IN 1..2 RETURN j) RETURN result IN test",
      ];

      queries.forEach(function (query) {
        let result = db._createStatement({query, bindVars: {}, options: paramEnabled}).explain();
        assertEqual([], result.plan.rules.filter((r) => r !== "splice-subqueries"), query);
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test that rule has an effect
    ////////////////////////////////////////////////////////////////////////////////

    testRuleHasEffect: function () {
      const queries = [
        "FOR i IN 1..10 LET result = IS_STRING(i) FILTER i < 10 RETURN result",
        "FOR i IN 1..10 LET result = IS_STRING(i) FILTER i < 10 FILTER i > 2 RETURN result",
        "FOR i IN 1..10 LET result = IS_STRING(i) FILTER i < 2 LIMIT 1 RETURN result",
        "FOR i IN 1..10 LET result = IS_STRING(i) LIMIT 1 RETURN result",
        "FOR i IN 1..10 LET result = IS_STRING(i) LET test = result + 1 LIMIT 1 RETURN test",
        "FOR i IN 1..10 LET result = IS_STRING(i) SORT i RETURN result",
        "FOR i IN 1..10 LET result = i + 1 LET test = (FOR j IN 1..2 RETURN j) RETURN result IN test",
        "FOR i IN 1..10 LET v = i * 2 LIMIT 2 LET result = v < 5 RETURN v",
        "FOR i IN 1..10 LET result = (RETURN IS_STRING(i)) FILTER i < 10 RETURN result",
        "FOR i IN 1..10 LET result = (RETURN IS_STRING(i)) FILTER i < 10 FILTER i > 2 RETURN result",
        "FOR i IN 1..10 LET result = (RETURN IS_STRING(i)) FILTER i < 2 LIMIT 1 RETURN result",
        "FOR i IN 1..10 LET result = (RETURN IS_STRING(i)) LIMIT 1 RETURN result",
        "FOR i IN 1..10 LET result = IS_STRING(i) LET test = (RETURN result + 1) LIMIT 1 RETURN test",
        "FOR i IN 1..10 LET result = (RETURN IS_STRING(i)) SORT i RETURN result",
        "FOR i IN 1..10 LET v = (RETURN i * 2) LIMIT 2 LET result = (RETURN v < 5) RETURN v",
      ];

      queries.forEach(function (query) {
        let result = db._createStatement({query, bindVars: {}, options: paramEnabled}).explain();
        assertEqual([ruleName], result.plan.rules.filter((r) => r !== "splice-subqueries"), query);
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test generated plans
    ////////////////////////////////////////////////////////////////////////////////

    testPlans: function () {
      const plans = [
        [
          "FOR i IN 1..10 LET result = IS_STRING(i) FILTER i < 10 RETURN result",
          [
            "SingletonNode",
            "CalculationNode",
            "EnumerateListNode",
            "CalculationNode",
            "FilterNode",
            "CalculationNode",
            "ReturnNode",
          ],
        ],
        [
          "FOR i IN 1..10 LET result = IS_STRING(i) FILTER i < 10 FILTER i > 2 RETURN result",
          [
            "SingletonNode",
            "CalculationNode",
            "EnumerateListNode",
            "CalculationNode",
            "FilterNode",
            "CalculationNode",
            "FilterNode",
            "CalculationNode",
            "ReturnNode",
          ],
        ],
        [
          "FOR i IN 1..10 LET result = IS_STRING(i) FILTER i < 2 LIMIT 1 RETURN result",
          [
            "SingletonNode",
            "CalculationNode",
            "EnumerateListNode",
            "CalculationNode",
            "FilterNode",
            "LimitNode",
            "CalculationNode",
            "ReturnNode",
          ],
        ],
        [
          "FOR i IN 1..10 LET result = IS_STRING(i) LIMIT 1 RETURN result",
          [
            "SingletonNode",
            "CalculationNode",
            "EnumerateListNode",
            "LimitNode",
            "CalculationNode",
            "ReturnNode",
          ],
        ],
        [
          "FOR i IN 1..10 LET result = IS_STRING(i) LET test = result + 1 LIMIT 1 RETURN test",
          [
            "SingletonNode",
            "CalculationNode",
            "EnumerateListNode",
            "LimitNode",
            "CalculationNode",
            "CalculationNode",
            "ReturnNode",
          ],
        ],
        [
          "FOR i IN 1..10 LET result = IS_STRING(i) SORT i RETURN result",
          [
            "SingletonNode",
            "CalculationNode",
            "EnumerateListNode",
            "SortNode",
            "CalculationNode",
            "ReturnNode",
          ],
        ],
        [
          "FOR i IN 1..10 LET v = i * 2 LIMIT 2 LET result = v < 5 RETURN v",
          [
            "SingletonNode",
            "CalculationNode",
            "EnumerateListNode",
            "LimitNode",
            "CalculationNode",
            "CalculationNode",
            "ReturnNode",
          ],
        ],
        [
          "FOR i IN 1..10 LET result = (RETURN IS_STRING(i)) FILTER i < 10 RETURN result",
          [
            "SingletonNode",
            "CalculationNode",
            "EnumerateListNode",
            "CalculationNode",
            "FilterNode",
            "SubqueryStartNode",
            "CalculationNode",
            "SubqueryEndNode",
            "ReturnNode",
          ],
        ],
        [
          "FOR i IN 1..10 LET result = (RETURN IS_STRING(i)) FILTER i < 10 FILTER i > 2 RETURN result",
          [
            "SingletonNode",
            "CalculationNode",
            "EnumerateListNode",
            "CalculationNode",
            "FilterNode",
            "CalculationNode",
            "FilterNode",
            "SubqueryStartNode",
            "CalculationNode",
            "SubqueryEndNode",
            "ReturnNode",
          ],
        ],
        [
          "FOR i IN 1..10 LET result = (RETURN IS_STRING(i)) FILTER i < 2 LIMIT 1 RETURN result",
          [
            "SingletonNode",
            "CalculationNode",
            "EnumerateListNode",
            "CalculationNode",
            "FilterNode",
            "LimitNode",
            "SubqueryStartNode",
            "CalculationNode",
            "SubqueryEndNode",
            "ReturnNode",
          ],
        ],
        [
          "FOR i IN 1..10 LET result = (RETURN IS_STRING(i)) LIMIT 1 RETURN result",
          [
            "SingletonNode",
            "CalculationNode",
            "EnumerateListNode",
            "LimitNode",
            "SubqueryStartNode",
            "CalculationNode",
            "SubqueryEndNode",
            "ReturnNode",
          ],
        ],
        [
          "FOR i IN 1..10 LET result = IS_STRING(i) LET test = (RETURN result + 1) LIMIT 1 RETURN test",
          [
            "SingletonNode",
            "CalculationNode",
            "EnumerateListNode",
            "LimitNode",
            "SubqueryStartNode",
            "CalculationNode",
            "CalculationNode",
            "SubqueryEndNode",
            "ReturnNode",
          ],
        ],
        [
          "FOR i IN 1..10 LET result = (RETURN IS_STRING(i)) SORT i RETURN result",
          [
            "SingletonNode",
            "CalculationNode",
            "EnumerateListNode",
            "SortNode",
            "SubqueryStartNode",
            "CalculationNode",
            "SubqueryEndNode",
            "ReturnNode",
          ],
        ],
        [
          "FOR i IN 1..10 LET v = (RETURN i * 2) LIMIT 2 LET result = (RETURN v < 5) RETURN v",
          [
            "SingletonNode",
            "CalculationNode",
            "EnumerateListNode",
            "LimitNode",
            "SubqueryStartNode",
            "CalculationNode",
            "SubqueryEndNode",
            "SubqueryStartNode",
            "CalculationNode",
            "SubqueryEndNode",
            "ReturnNode",
          ],
        ],
      ];

      plans.forEach(function (plan) {
        let result = db._createStatement({query: plan[0], bindVars: {}, options: paramEnabled}).explain();
        assertEqual([ruleName], result.plan.rules.filter((r) => r !== "splice-subqueries"), plan[0]);
        assertEqual(
          plan[1],
          helper.getCompactPlan(result).map(function (node) {
            return node.type;
          }),
          plan
        );
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test results
    ////////////////////////////////////////////////////////////////////////////////

    testResultsNoEffect: function () {
      const queries = [
        [
          "FOR i IN 1..10 LET a = (RETURN i + 1) LET x = (FOR j IN 1..i RETURN j) RETURN a[0] - 1 IN x ? 1 : 0",
          [1, 1, 1, 1, 1, 1, 1, 1, 1, 1],
        ],
        [
          "FOR i IN 1..10 LET a = (RETURN i + 1) LET x = (FOR j IN 1..i RETURN j) RETURN a[0] IN x ? 1 : 0",
          [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
        ],
      ];

      queries.forEach(function (query) {
        const planDisabled = db._createStatement({query: query[0], bindVars: {}, options: paramDisabled}).explain();
        const planEnabled = db._createStatement({query: query[0], bindVars: {}, options: paramEnabled}).explain();

        // the rule should not trigger in these queries, so everything should be identical
        const resultDisabled = db._query(query[0], {}, paramDisabled);
        const resultEnabled = db._query(query[0], {}, paramEnabled);
        
        assertEqual(-1, planDisabled.plan.rules.indexOf(ruleName), query[0]);
        assertEqual(-1, planEnabled.plan.rules.indexOf(ruleName), query[0]);

        assertEqual(resultDisabled.toArray(), query[1], query[0]);
        assertEqual(resultEnabled.toArray(), query[1], query[0]);
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test results
    ////////////////////////////////////////////////////////////////////////////////

    testResults: function () {
      const queries = [
        ["FOR i IN 1..10 LET a = i + 1 FILTER i < 4 RETURN a", [2, 3, 4]],
        [
          "FOR i IN 1..10 LET a = i + 1 FILTER i < 7 FILTER i > 1 RETURN a",
          [3, 4, 5, 6, 7],
        ],
        ["FOR i IN 1..10 LET a = i + 1 LIMIT 4 RETURN a", [2, 3, 4, 5]],
        [
          "FOR i IN 1..10 LET a = i + 1 LET b = a + 1 FILTER i < 3 RETURN b",
          [3, 4],
        ],
        [
          "FOR i IN 1..10 LET a = i + 1 LET b = a + 1 LIMIT 4 RETURN b",
          [3, 4, 5, 6],
        ],
        [
          "FOR i IN 1..10 LET a = i + 1 LET b = a + 1 FILTER i < 5 LIMIT 4 RETURN b",
          [3, 4, 5, 6],
        ],
        [
          "FOR i IN 1..10 LET a = i + 1 LET x = (FOR j IN 1..i RETURN j) RETURN a - 1 IN x ? 1 : 0",
          [1, 1, 1, 1, 1, 1, 1, 1, 1, 1],
        ],
        [
          "FOR i IN 1..10 LET a = i + 1 LET x = (FOR j IN 1..i RETURN j) RETURN a IN x ? 1 : 0",
          [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
        ],
        [
          "FOR i IN 1..10 LET a = MAX(1..i) FILTER i > 4 RETURN a",
          [5, 6, 7, 8, 9, 10],
        ],
        ["FOR i IN 1..10 LET a = SUM(1..i) FILTER i == 3 RETURN a", [6]],
        [
          "FOR i IN 1..10 LET a = SUM(1..i) LET b = MIN(1..i) FILTER i == 3 RETURN [ a, b ]",
          [[6, 1]],
        ],
        ["FOR i IN 1..10 LET a = SUM(1..i) LIMIT 3 RETURN a", [1, 3, 6]],
        [
          "FOR i IN 1..10 LET a = SUM(1..i) LIMIT 3 FILTER a > 2 RETURN a",
          [3, 6],
        ],
        [
          "FOR i IN 1..10 LET a = SUM(1..i) LIMIT 5 FILTER i > 2 RETURN a",
          [6, 10, 15],
        ],
        [
          "FOR i IN 1..10 LET a = (RETURN i + 1) FILTER i < 4 RETURN a[0]",
          [2, 3, 4],
        ],
        [
          "FOR i IN 1..10 LET a = (RETURN i + 1) FILTER i < 7 FILTER i > 1 RETURN a[0]",
          [3, 4, 5, 6, 7],
        ],
        [
          "FOR i IN 1..10 LET a = (RETURN i + 1) LIMIT 4 RETURN a[0]",
          [2, 3, 4, 5],
        ],
        [
          "FOR i IN 1..10 LET a = (RETURN i + 1) LET b = (RETURN a[0] + 1) FILTER i < 3 RETURN b[0]",
          [3, 4],
        ],
        [
          "FOR i IN 1..10 LET a = i + 1 LET b = (RETURN a + 1) LIMIT 4 RETURN b[0]",
          [3, 4, 5, 6],
        ],
        [
          "FOR i IN 1..10 LET a = (RETURN i + 1) LET b = (RETURN a[0] + 1) FILTER i < 5 LIMIT 4 RETURN b[0]",
          [3, 4, 5, 6],
        ],
        [
          "FOR i IN 1..10 LET a = (RETURN MAX(1..i)) FILTER i > 4 RETURN a[0]",
          [5, 6, 7, 8, 9, 10],
        ],
        [
          "FOR i IN 1..10 LET a = (RETURN SUM(1..i)) FILTER i == 3 RETURN a[0]",
          [6],
        ],
        [
          "FOR i IN 1..10 LET a = (RETURN SUM(1..i)) LET b = (RETURN MIN(1..i)) FILTER i == 3 RETURN [ a[0], b[0] ]",
          [[6, 1]],
        ],
        [
          "FOR i IN 1..10 LET a = (RETURN SUM(1..i)) LIMIT 3 RETURN a[0]",
          [1, 3, 6],
        ],
        [
          "FOR i IN 1..10 LET a = (RETURN SUM(1..i)) LIMIT 3 FILTER a[0] > 2 RETURN a[0]",
          [3, 6],
        ],
        [
          "FOR i IN 1..10 LET a = (RETURN SUM(1..i)) LIMIT 5 FILTER i > 2 RETURN a[0]",
          [6, 10, 15],
        ],
      ];

      queries.forEach(function (query) {
        const planDisabled = db._createStatement({query: query[0], bindVars: {}, options: paramDisabled}).explain();
        const planEnabled = db._createStatement({query: query[0], bindVars: {}, options: paramEnabled}).explain();

        const resultDisabled = db._query(query[0], {}, paramDisabled);
        const resultEnabled = db._query(query[0], {}, paramEnabled);
        
        assertEqual(-1, planDisabled.plan.rules.indexOf(ruleName), query[0]);
        assertNotEqual(-1, planEnabled.plan.rules.indexOf(ruleName), query[0]);

        assertEqual(resultDisabled.toArray(), query[1], query[0]);
        assertEqual(resultEnabled.toArray(), query[1], query[0]);
      });
    },

    testComplexQuery : function () {
      // the following subqueries are all uncorrelated, so they should
      // not be moved around
      const query = `
FOR n IN UNION(
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc,
  FOR doc IN [1,2,3] FILTER doc IN [1,2,3] LIMIT 1 RETURN doc
) LIMIT 1 RETURN 1
`;

      const plan = db._createStatement({query}).explain();

      // the rule should not trigger in these queries, so everything should be identical
      assertEqual(-1, plan.plan.rules.indexOf(ruleName), query);
      
      const result = db._query(query).toArray();

      assertEqual([1], result, query);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test results
    ////////////////////////////////////////////////////////////////////////////////

    testCollection1: function () {
      let expected = [];
      for (let i = 0; i < 100; ++i) {
        expected.push("test" + i + "-" + i);
      }

      const query =
        "FOR i IN " +
        cn +
        " LET result = CONCAT(i._key, '-', i.value) SORT i.value RETURN result";
      const planDisabled = db._createStatement({query, bindVars: {}, options: paramDisabled}).explain();
      const planEnabled = db._createStatement({query, bindVars: {}, options: paramEnabled}).explain();

      const resultDisabled = db._query(query, {}, paramDisabled);
      const resultEnabled = db._query(query, {}, paramEnabled);

      assertEqual(-1, planDisabled.plan.rules.indexOf(ruleName), query[0]);
      assertNotEqual(-1, planEnabled.plan.rules.indexOf(ruleName), query[0]);

      assertEqual(resultDisabled.toArray(), expected, query[0]);
      assertEqual(resultEnabled.toArray(), expected, query[0]);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test results
    ////////////////////////////////////////////////////////////////////////////////

    testCollection2: function () {
      const expected = ["test43-43", "test44-44"];

      const query =
        "FOR i IN " +
        cn +
        " LET result = CONCAT(i._key, '-', i.value) FILTER i.value > 42 SORT i.value LIMIT 2 RETURN result";
      const planDisabled = db._createStatement({query, bindVars: {}, options: paramDisabled}).explain();
      const planEnabled = db._createStatement({query, bindVars: {}, options: paramEnabled}).explain();

      const resultDisabled = db._query(query, {}, paramDisabled);
      const resultEnabled = db._query(query, {}, paramEnabled);

      assertEqual(-1, planDisabled.plan.rules.indexOf(ruleName), query);
      assertNotEqual(-1, planEnabled.plan.rules.indexOf(ruleName), query);

      assertEqual(resultDisabled.toArray(), expected, query);
      assertEqual(resultEnabled.toArray(), expected, query);
    },

    testCollection3: function () {
      const expected = ["test0-0", "test1-1"];

      const query =
        "FOR i IN " +
        cn +
        " LET result = CONCAT(i._key, '-', i.value) SORT i.value LIMIT 2 RETURN result";
      const planDisabled = db._createStatement({query, bindVars: {}, options: paramDisabled}).explain();
      const planEnabled = db._createStatement({query, bindVars: {}, options: paramEnabled}).explain();

      const resultDisabled = db._query(query, {}, paramDisabled);
      const resultEnabled = db._query(query, {}, paramEnabled);

      assertEqual(-1, planDisabled.plan.rules.indexOf(ruleName), query);
      assertNotEqual(-1, planEnabled.plan.rules.indexOf(ruleName), query);

      assertEqual(resultDisabled.toArray(), expected, query);
      assertEqual(resultEnabled.toArray(), expected, query);
    },

    testCollection4: function () {
      let expected = [];
      for (let i = 0; i < 100; ++i) {
        expected.push("test" + i + "-" + i);
      }

      const query =
        "FOR i IN " +
        cn +
        " LET result = (RETURN CONCAT(i._key, '-', i.value)) SORT i.value RETURN result[0]";
      const planDisabled = db._createStatement({query, bindVars: {}, options: paramDisabled}).explain();
      const planEnabled = db._createStatement({query, bindVars: {}, options: paramEnabled}).explain();

      const resultDisabled = db._query(query, {}, paramDisabled);
      const resultEnabled = db._query(query, {}, paramEnabled);

      assertEqual(-1, planDisabled.plan.rules.indexOf(ruleName), query[0]);
      assertNotEqual(-1, planEnabled.plan.rules.indexOf(ruleName), query[0]);

      assertEqual(resultDisabled.toArray(), expected, query[0]);
      assertEqual(resultEnabled.toArray(), expected, query[0]);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test results
    ////////////////////////////////////////////////////////////////////////////////

    testCollection5: function () {
      const expected = ["test43-43", "test44-44"];

      const query =
        "FOR i IN " +
        cn +
        " LET result = (RETURN CONCAT(i._key, '-', i.value)) FILTER i.value > 42 SORT i.value LIMIT 2 RETURN result[0]";
      const planDisabled = db._createStatement({query, bindVars: {}, options: paramDisabled}).explain();
      const planEnabled = db._createStatement({query, bindVars: {}, options: paramEnabled}).explain();

      const resultDisabled = db._query(query, {}, paramDisabled);
      const resultEnabled = db._query(query, {}, paramEnabled);

      assertEqual(-1, planDisabled.plan.rules.indexOf(ruleName), query);
      assertNotEqual(-1, planEnabled.plan.rules.indexOf(ruleName), query);

      assertEqual(resultDisabled.toArray(), expected, query);
      assertEqual(resultEnabled.toArray(), expected, query);
    },

    testCollection6: function () {
      const expected = ["test0-0", "test1-1"];

      const query =
        "FOR i IN " +
        cn +
        " LET result = (RETURN CONCAT(i._key, '-', i.value)) SORT i.value LIMIT 2 RETURN result[0]";
      const planDisabled = db._createStatement({query, bindVars: {}, options: paramDisabled}).explain();
      const planEnabled = db._createStatement({query, bindVars: {}, options: paramEnabled}).explain();

      const resultDisabled = db._query(query, {}, paramDisabled);
      const resultEnabled = db._query(query, {}, paramEnabled);

      assertEqual(-1, planDisabled.plan.rules.indexOf(ruleName), query);
      assertNotEqual(-1, planEnabled.plan.rules.indexOf(ruleName), query);

      assertEqual(resultDisabled.toArray(), expected, query);
      assertEqual(resultEnabled.toArray(), expected, query);
    },

    testModify: function () {
      let expected = [];
      for (let i = 0; i < 10; ++i) {
        expected.push("test" + i + "-" + i);
      }

      const query = `
        FOR i IN 0..99
          LET result = (
            UPDATE {_key: CONCAT('test', TO_STRING(i))} WITH {updated: true} IN ${cn}
            RETURN CONCAT(NEW._key, '-', NEW.value)
          )
          LIMIT 10
          RETURN result[0]
      `;
      const planDisabled = db._createStatement({query, bindVars: {}, options: paramDisabled}).explain();
      const planEnabled = db._createStatement({query, bindVars: {}, options: paramEnabled}).explain();

      const resultDisabled = db._query(query, {}, paramDisabled);
      const resultEnabled = db._query(query, {}, paramEnabled);

      assertEqual(-1, planDisabled.plan.rules.indexOf(ruleName), query[0]);
      assertEqual(-1, planEnabled.plan.rules.indexOf(ruleName), query[0]);

      assertEqual(resultDisabled.toArray(), expected, query[0]);
      assertEqual(resultEnabled.toArray(), expected, query[0]);
    },

    testNestedModify: function () {
      let expected = [];
      for (let i = 0; i < 100; ++i) {
        expected.push([[`test${i}-${i}`]]);
      }

      const query = `FOR i IN 0..99
          LET outerSubquery = (
            LET result = (
              UPDATE {_key: CONCAT('test', TO_STRING(i))} WITH {updated: true} IN ${cn}
              RETURN CONCAT(NEW._key, '-', NEW.value)
            )
            RETURN result
          )
          RETURN outerSubquery`;
      for (const params of RulesCombinator([
        "move-calculations-down",
      ])) {
        const { plan } = db._createStatement({query, bindVars: {}, options: params}).explain();
        let res = db._query(query, {}, params);
        const json = res.toArray();
        const stats = res.getExtra().stats;
        const { writesExecuted } = stats;
        assertEqual(100, writesExecuted, { query, params });
        assertEqual(json, expected, { query, params });
      }
    },

    testNestedModifyLimit: function () {
      let expected = [];
      for (let i = 1; i < 3; ++i) {
        expected.push([[`test${i}-${i}`]]);
      }

      const query = `FOR i IN 0..99
          LET outerSubquery = (
            LET result = (
              UPDATE {_key: CONCAT('test', TO_STRING(i))} WITH {updated: true} IN ${cn}
              RETURN CONCAT(NEW._key, '-', NEW.value)
            )
            RETURN result
          )
          LIMIT 1, 2
          RETURN outerSubquery`;
      for (const params of RulesCombinator([
        "move-calculations-down",
      ])) {
        const { plan } = db._createStatement({query, bindVars: {}, options: params}).explain();
        let res = db._query(query, {}, params);
        const json = res.toArray();
        const stats = res.getExtra().stats;
        const { writesExecuted } = stats;
        assertEqual(100, writesExecuted, { query, params });
        assertEqual(json, expected, { query, params });
      }
    },

    testNestedModifyLimitFilterTakeAll: function () {
      let expected = [];
      for (let i = 1; i < 3; ++i) {
        expected.push([[`test${i}-${i}`]]);
      }

      const query = `FOR i IN 0..99
          LET outerSubquery = (
            LET result = (
              UPDATE {_key: CONCAT('test', TO_STRING(i))} WITH {updated: true} IN ${cn}
              RETURN CONCAT(NEW._key, '-', NEW.value)
            )
            RETURN result
          )
          FILTER LENGTH(outerSubquery) == 1
          LIMIT 1, 2
          RETURN outerSubquery`;
      for (const params of RulesCombinator([
        "move-calculations-down",
      ])) {
        const { plan } = db._createStatement({query, bindVars: {}, options: params}).explain();
        let res = db._query(query, {}, params);
        const json = res.toArray();
        const stats = res.getExtra().stats;
        const { writesExecuted } = stats;
        assertEqual(100, writesExecuted, { query, params });
        assertEqual(json, expected, { query, params });
      }
    },

    testNestedModifyLimitFilterTakeNone: function () {
      const expected = [];

      const query = `FOR i IN 0..99
          LET outerSubquery = (
            LET result = (
              UPDATE {_key: CONCAT('test', TO_STRING(i))} WITH {updated: true} IN ${cn}
              RETURN CONCAT(NEW._key, '-', NEW.value)
            )
            RETURN result
          )
          FILTER LENGTH(outerSubquery) > 1
          LIMIT 1, 2
          RETURN outerSubquery`;
      for (const params of RulesCombinator([
        "move-calculations-down",
      ])) {
        const { plan } = db._createStatement({query, bindVars: {}, options: params}).explain();
        let res = db._query(query, {}, params);
        const json = res.toArray();
        const stats = res.getExtra().stats;
        const { writesExecuted } = stats;
        assertEqual(100, writesExecuted, { query, params });
        assertEqual(json, expected, { query, params });
      }
    },
  };
}

jsunity.run(optimizerRuleTestSuite);

return jsunity.done();
