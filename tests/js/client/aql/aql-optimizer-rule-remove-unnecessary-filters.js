/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse */

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

var jsunity = require("jsunity");
var helper = require("@arangodb/aql-helper");
var isEqual = helper.isEqual;
const db = require('internal').db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerRuleTestSuite () {
  var ruleName = "remove-unnecessary-filters";
  // various choices to control the optimizer: 
  var paramNone     = { optimizer: { rules: [ "-all" ] } };
  var paramEnabled  = { optimizer: { rules: [ "-all", "+" + ruleName ] } };
  var paramDisabled = { optimizer: { rules: [ "+all", "-" + ruleName ] } };
  var paramMore     = { optimizer: { rules: [ "-all", "+" + ruleName, "+remove-unnecessary-calculations-2" ] } };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has no effect when explicitly disabled
////////////////////////////////////////////////////////////////////////////////

    testRuleDisabled : function () {
      var queries = [ 
        "FOR i IN 1..10 FILTER true RETURN 1",
        "FOR i IN 1..10 FILTER 1 != 7 RETURN 1",
        "FOR i IN 1..10 FILTER 1 == 1 && 2 == 2 RETURN 1",
        "FOR i IN 1..10 FILTER 1 != 1 && 2 != 2 RETURN 1"
      ];

      queries.forEach(function(query) {
        var result = db._createStatement({query: query, bindVars:  { }, options:  paramNone}).explain();
        assertEqual([ ], result.plan.rules);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has no effect
////////////////////////////////////////////////////////////////////////////////

    testRuleNoEffect : function () {
      var queries = [ 
        "FOR i IN 1..10 FILTER i > 1 RETURN i",
        "FOR i IN 1..10 LET a = 99 FILTER i > a RETURN i",
        "FOR i IN 1..10 LET a = i FILTER a != 99 RETURN i"
      ];

      queries.forEach(function(query) {
        var result = db._createStatement({query: query, bindVars:  { }, options:  paramEnabled}).explain();
        assertEqual([ ], result.plan.rules, query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has an effect
////////////////////////////////////////////////////////////////////////////////

    testRuleHasEffect : function () {
      var queries = [ 
        [ "FOR i IN 1..2 FILTER true RETURN i", true ],
        [ "FOR i IN 1..2 FILTER 1 > 9 RETURN i", false ],
        [ "FOR i IN 1..2 FILTER 1 < 9 RETURN i", true ],
        [ "FOR i IN 1..2 LET a = 1 FILTER a == 1 RETURN i", true ],
        [ "FOR i IN 1..2 LET a = 1 LET b = 1 FILTER a == b RETURN i", true ],
        [ "FOR i IN 1..2 LET a = 1 LET b = 1 FILTER a != b RETURN i", false ],
        [ "FOR i IN 1..2 LET a = 1 LET b = 2 FILTER a != b RETURN i", true ],
        [ "FOR i IN 1..2 LET a = 1 LET b = 2 FILTER a == b RETURN i", false ],
        [ "FOR i IN 1..2 FILTER false RETURN i", false ],
        [ "FOR i IN 1..2 LET a = 1 FILTER a == 9 RETURN i", false ],
        [ "FOR i IN 1..2 LET a = 1 FILTER a != 1 RETURN i", false ],
        [ "FOR i IN 1..2 FILTER 1 == 1 && 2 == 2 RETURN i", true ],
        [ "FOR i IN 1..2 FILTER 1 != 1 && 2 != 2 RETURN i", false ],
      ];

      queries.forEach(function(query) {
        var result = db._createStatement({query: query[0], bindVars:  { }, options:  paramEnabled}).explain();
        assertEqual([ ], result.plan.rules, query);
        result = db._query(query[0], { }, paramEnabled).toArray();
        if (query[1]) {
          assertEqual([ 1, 2 ], result, query);
        } else {
          assertEqual([ ], result, query);
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test generated plans
////////////////////////////////////////////////////////////////////////////////


    testPlans : function () {
      var plans = [ 
        [ "FOR i IN 1..10 FILTER true RETURN i", [ "SingletonNode", "CalculationNode", "EnumerateListNode", "ReturnNode" ] ],
        [ "FOR i IN 1..10 FILTER 1 < 9 RETURN i", [ "SingletonNode", "CalculationNode", "EnumerateListNode", "ReturnNode" ] ],
        [ "FOR i IN 1..10 LET a = 1 FILTER a == 1 RETURN i", [ "SingletonNode", "CalculationNode", "EnumerateListNode", "ReturnNode" ] ],
        [ "FOR i IN 1..10 LET a = 1 LET b = 1 FILTER a == b RETURN i", [ "SingletonNode", "CalculationNode", "EnumerateListNode", "ReturnNode" ] ],
        [ "FOR i IN 1..10 LET a = 1 LET b = 2 FILTER a != b RETURN i", [ "SingletonNode", "CalculationNode", "EnumerateListNode", "ReturnNode" ] ],
        [ "FOR i IN 1..10 FILTER false RETURN i", [ "SingletonNode", "CalculationNode", "EnumerateListNode", "NoResultsNode", "ReturnNode" ] ],
        [ "FOR i IN 1..10 LET a = 1 FILTER a == 9 RETURN i", [ "SingletonNode", "CalculationNode", "EnumerateListNode", "NoResultsNode", "ReturnNode" ] ],
        [ "FOR i IN 1..10 LET a = 1 FILTER a != 1 RETURN i", [ "SingletonNode", "CalculationNode", "EnumerateListNode", "NoResultsNode", "ReturnNode" ] ]
      ];

      plans.forEach(function(plan) {
        var result = db._createStatement({query: plan[0], bindVars:  { }, options:  paramMore}).explain();
        // rule will not fire anymore for constant filters
        assertFalse(result.plan.rules.indexOf(ruleName) !== -1, plan[0]);
        assertEqual(plan[1], helper.getCompactPlan(result).map(function(node) { return node.type; }), plan[0]);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test results
////////////////////////////////////////////////////////////////////////////////

    testResults : function () {
      var queries = [ 
        [ "FOR i IN 1..10 FILTER true RETURN i", [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] ],
        [ "FOR i IN 1..10 LET a = 1 FILTER a == 1 RETURN i", [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] ],
        [ "FOR i IN 1..10 LET a = 1 FILTER a != 99 RETURN i", [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] ],
        [ "FOR i IN 1..10 LET a = 1 LET b = 1 FILTER a == b RETURN i", [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] ],
        [ "FOR i IN 1..10 FILTER false RETURN i", [ ] ],
        [ "FOR i IN 1..10 FILTER 1 == 7 RETURN i", [ ] ],
        [ "LET a = 1 FOR i IN 1..10 FILTER a == 7 && a == 3 RETURN i", [ ] ],
        [ "FOR i IN 1..10 LET a = 1 FILTER a == 7 RETURN i", [ ] ],
        [ "FOR i IN 1..10 LET a = 1 FILTER a != 1 RETURN i", [ ] ]
      ];

      queries.forEach(function(query) {
        var planDisabled   = db._createStatement({query: query[0], bindVars:  { }, options:  paramDisabled}).explain();
        var planEnabled    = db._createStatement({query: query[0], bindVars:  { }, options:  paramEnabled}).explain();
        var resultDisabled = db._query(query[0], { }, paramDisabled).toArray();
        var resultEnabled  = db._query(query[0], { }, paramEnabled).toArray();

        assertTrue(isEqual(resultDisabled, resultEnabled), query[0]);

        assertTrue(planDisabled.plan.rules.indexOf(ruleName) === -1, query[0]);
        // rule will not fire anymore for constant filters
        assertTrue(planEnabled.plan.rules.indexOf(ruleName) === -1, query[0]);

        assertEqual(resultDisabled, query[1]);
        assertEqual(resultEnabled, query[1]);
      });
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for empty-array comparison constant folding
////////////////////////////////////////////////////////////////////////////////

function emptyArrayFilterSuite() {
  const cn = 'UnitTestsEmptyArrayFilter';
  const paramNoRules = { optimizer: { rules: ['-all'] } };

  return {
    setUpAll: function () {
      db._drop(cn);
      const c = db._create(cn);
      const docs = [];
      for (let i = 0; i < 5; ++i) {
        docs.push({ value: i });
      }
      c.insert(docs);
    },

    tearDownAll: function () {
      db._drop(cn);
    },

    // x IN [] is always false — handled at plan construction, produces NoResultsNode
    testInEmptyArrayDirectAlwaysFalse: function () {
      const q = `FOR doc IN ${cn} FILTER doc.value IN [] RETURN doc`;
      const explain = db._createStatement({query: q, bindVars: {}, options: paramNoRules}).explain();
      const nodeTypes = helper.getCompactPlan(explain).map(n => n.type);
      assertTrue(nodeTypes.indexOf('NoResultsNode') !== -1, nodeTypes);
      assertTrue(nodeTypes.indexOf('FilterNode') === -1, nodeTypes);
      assertEqual([], db._query(q).toArray());
    },

    // via a LET variable — same folding applies
    testInEmptyArrayViaLet: function () {
      const q = `FOR i IN 1..10 LET x = i IN [] FILTER x RETURN i`;
      const explain = db._createStatement({query: q, bindVars: {}, options: paramNoRules}).explain();
      const nodeTypes = helper.getCompactPlan(explain).map(n => n.type);
      assertTrue(nodeTypes.indexOf('NoResultsNode') !== -1, nodeTypes);
      assertTrue(nodeTypes.indexOf('FilterNode') === -1, nodeTypes);
      assertEqual([], db._query(q).toArray());
    },

    // NOT (x IN []) — NOT of always-false is always true, filter removed
    testNotOfInEmptyArrayAlwaysTrue: function () {
      const q = `FOR doc IN ${cn} FILTER NOT (doc.value IN []) RETURN doc`;
      const explain = db._createStatement({query: q, bindVars: {}, options: paramNoRules}).explain();
      const nodeTypes = helper.getCompactPlan(explain).map(n => n.type);
      assertTrue(nodeTypes.indexOf('FilterNode') === -1, nodeTypes);
      assertTrue(nodeTypes.indexOf('NoResultsNode') === -1, nodeTypes);
      assertEqual(5, db._query(q).toArray().length);
    },

    // x NOT IN [] is always true, filter removed
    testNotInEmptyArrayDirectAlwaysTrue: function () {
      const q = `FOR doc IN ${cn} FILTER doc.value NOT IN [] RETURN doc`;
      const nodeTypes = helper.getCompactPlan(
        db._createStatement({query: q, bindVars: {}, options: paramNoRules}).explain()
      ).map(n => n.type);
      assertTrue(nodeTypes.indexOf('FilterNode') === -1, nodeTypes);
      assertTrue(nodeTypes.indexOf('NoResultsNode') === -1, nodeTypes);
      assertEqual(5, db._query(q, {}, paramNoRules).toArray().length);
    },

    // x IN [] AND y > 0 — AND with always-false left member is always false
    testInEmptyArrayInAndAlwaysFalse: function () {
      const q = `FOR doc IN ${cn} FILTER doc.value IN [] AND doc.value > 0 RETURN doc`;
      const explain = db._createStatement({query: q, bindVars: {}, options: paramNoRules}).explain();
      const nodeTypes = helper.getCompactPlan(explain).map(n => n.type);
      assertTrue(nodeTypes.indexOf('NoResultsNode') !== -1, nodeTypes);
      assertTrue(nodeTypes.indexOf('FilterNode') === -1, nodeTypes);
      assertEqual([], db._query(q).toArray());
    },

    // y > 0 AND x IN [] — AND with always-false right member is always false
    testInEmptyArrayAsSecondAndOperandAlwaysFalse: function () {
      const q = `FOR doc IN ${cn} FILTER doc.value > 0 AND doc.value IN [] RETURN doc`;
      const explain = db._createStatement({query: q, bindVars: {}, options: paramNoRules}).explain();
      const nodeTypes = helper.getCompactPlan(explain).map(n => n.type);
      assertTrue(nodeTypes.indexOf('NoResultsNode') !== -1, nodeTypes);
      assertTrue(nodeTypes.indexOf('FilterNode') === -1, nodeTypes);
      assertEqual([], db._query(q).toArray());
    },

    // non-empty IN must not be folded to false
    testNonEmptyArrayInNotFolded: function () {
      const q = `FOR doc IN ${cn} FILTER doc.value IN [0, 1] RETURN doc`;
      const explain = db._createStatement({query: q, bindVars: {}, options: paramNoRules}).explain();
      const nodeTypes = helper.getCompactPlan(explain).map(n => n.type);
      assertTrue(nodeTypes.indexOf('NoResultsNode') === -1, nodeTypes);
      assertTrue(nodeTypes.indexOf('FilterNode') !== -1, nodeTypes);
      assertEqual(2, db._query(q).toArray().length);
    },

    // [] ANY == x (direct FILTER): folded at AST optimization time, no rules needed
    testAnyEqEmptyArrayDirectFoldedAtAstLevel: function () {
      const q = `FOR doc IN ${cn} FILTER [] ANY == doc.value RETURN doc`;
      const nodeTypes = helper.getCompactPlan(
        db._createStatement({query: q, bindVars: {}, options: paramNoRules}).explain()
      ).map(n => n.type);
      assertTrue(nodeTypes.indexOf('NoResultsNode') !== -1, nodeTypes);
      assertTrue(nodeTypes.indexOf('FilterNode') === -1, nodeTypes);
      assertEqual([], db._query(q, {}, paramNoRules).toArray());
    },

    // via LET — variable ref bypasses plan construction fold; rule catches it
    testAnyEqEmptyArrayViaLetFoldedByRule: function () {
      const opts = { optimizer: { rules: ['-all', '+remove-unnecessary-filters-2'] } };
      const q = `FOR doc IN ${cn} LET y = ([] ANY == doc.value) FILTER y RETURN doc`;
      const nodeTypes = helper.getCompactPlan(
        db._createStatement({query: q, bindVars: {}, options: opts}).explain()
      ).map(n => n.type);
      assertTrue(nodeTypes.indexOf('NoResultsNode') !== -1, nodeTypes);
      assertTrue(nodeTypes.indexOf('FilterNode') === -1, nodeTypes);
      assertEqual([], db._query(q, {}, opts).toArray());
    },

    // same via LET, with replace-any-eq-with-in also enabled — rewrite chain, same result
    testAnyEqEmptyArrayViaLetChainedOptimization: function () {
      const opts = { optimizer: { rules: ['-all', '+remove-unnecessary-filters-2', '+replace-any-eq-with-in'] } };
      const q = `FOR doc IN ${cn} LET y = ([] ANY == doc.value) FILTER y RETURN doc`;
      const nodeTypes = helper.getCompactPlan(
        db._createStatement({query: q, bindVars: {}, options: opts}).explain()
      ).map(n => n.type);
      assertTrue(nodeTypes.indexOf('NoResultsNode') !== -1, nodeTypes);
      assertTrue(nodeTypes.indexOf('FilterNode') === -1, nodeTypes);
      assertEqual([], db._query(q, {}, opts).toArray());
    },

    // NOOPT prevents AST folding; plan construction still catches it
    testNoOptInEmptyArrayProducesNoResults: function () {
      const q = `FOR doc IN ${cn} FILTER NOOPT(doc.value) IN [] RETURN doc`;
      const nodeTypes = helper.getCompactPlan(
        db._createStatement({query: q, bindVars: {}, options: paramNoRules}).explain()
      ).map(n => n.type);
      assertTrue(nodeTypes.indexOf('NoResultsNode') !== -1, nodeTypes);
      assertTrue(nodeTypes.indexOf('FilterNode') === -1, nodeTypes);
      assertEqual([], db._query(q, {}, paramNoRules).toArray());
    },

    // NOT ([] ANY == x) — NOT of always-false is always true, filter removed
    testNotOfAnyEqEmptyArrayAlwaysTrue: function () {
      const q = `FOR doc IN ${cn} FILTER NOT ([] ANY == doc.value) RETURN doc`;
      const nodeTypes = helper.getCompactPlan(
        db._createStatement({query: q, bindVars: {}, options: paramNoRules}).explain()
      ).map(n => n.type);
      assertTrue(nodeTypes.indexOf('FilterNode') === -1, nodeTypes);
      assertTrue(nodeTypes.indexOf('NoResultsNode') === -1, nodeTypes);
      assertEqual(5, db._query(q, {}, paramNoRules).toArray().length);
    },

    // NOOPT prevents AST folding; plan construction still catches it
    testNoOptNotInEmptyArrayAlwaysTrue: function () {
      const q = `FOR doc IN ${cn} FILTER NOOPT(doc.value) NOT IN [] RETURN doc`;
      const nodeTypes = helper.getCompactPlan(
        db._createStatement({query: q, bindVars: {}, options: paramNoRules}).explain()
      ).map(n => n.type);
      assertTrue(nodeTypes.indexOf('FilterNode') === -1, nodeTypes);
      assertTrue(nodeTypes.indexOf('NoResultsNode') === -1, nodeTypes);
      assertEqual(5, db._query(q, {}, paramNoRules).toArray().length);
    },

    // non-empty ALL/NONE must NOT be folded
    testNonEmptyAllNoneNotFolded: function () {
      // [0,1] ALL == x: impossible (no single value equals both), returns nothing
      // [0,1] NONE == x: docs where value ∉ {0,1}, so values 2,3,4 → 3 results
      const cases = [
        { q: `FOR doc IN ${cn} FILTER [0, 1] ALL == doc.value RETURN doc`, expected: 0 },
        { q: `FOR doc IN ${cn} FILTER [0, 1] NONE == doc.value RETURN doc`, expected: 3 },
      ];
      cases.forEach(function({ q, expected }) {
        const nodeTypes = helper.getCompactPlan(
          db._createStatement({query: q, bindVars: {}, options: paramNoRules}).explain()
        ).map(n => n.type);
        assertTrue(nodeTypes.indexOf('NoResultsNode') === -1, q);
        assertTrue(nodeTypes.indexOf('FilterNode') !== -1, q);
        assertEqual(expected, db._query(q, {}, paramNoRules).toArray().length, q);
      });
    },

    // [0,1,2] ANY == x — non-empty array must NOT be folded to false
    testNonEmptyAnyEqNotFolded: function () {
      const q = `FOR doc IN ${cn} FILTER [0, 1, 2] ANY == doc.value RETURN doc`;
      const nodeTypes = helper.getCompactPlan(
        db._createStatement({query: q, bindVars: {}, options: paramNoRules}).explain()
      ).map(n => n.type);
      assertTrue(nodeTypes.indexOf('NoResultsNode') === -1, nodeTypes);
      assertTrue(nodeTypes.indexOf('FilterNode') !== -1, nodeTypes);
      assertEqual(3, db._query(q).toArray().length);
    },

    // [] ANY <op> x — always false regardless of operator
    testAnyEmptyArrayOtherOpsAlwaysFalse: function () {
      const queries = [
        `FOR doc IN ${cn} FILTER [] ANY != doc.value RETURN doc`,
        `FOR doc IN ${cn} FILTER [] ANY > doc.value RETURN doc`,
        `FOR doc IN ${cn} FILTER [] ANY < doc.value RETURN doc`,
        `FOR doc IN ${cn} FILTER [] ANY >= doc.value RETURN doc`,
        `FOR doc IN ${cn} FILTER [] ANY <= doc.value RETURN doc`,
        `FOR doc IN ${cn} FILTER [] ANY IN doc.value RETURN doc`,
        `FOR doc IN ${cn} FILTER [] ANY NOT IN doc.value RETURN doc`,
      ];
      queries.forEach(function(q) {
        const nodeTypes = helper.getCompactPlan(
          db._createStatement({query: q, bindVars: {}, options: paramNoRules}).explain()
        ).map(n => n.type);
        assertTrue(nodeTypes.indexOf('NoResultsNode') !== -1, q);
        assertTrue(nodeTypes.indexOf('FilterNode') === -1, q);
        assertEqual([], db._query(q, {}, paramNoRules).toArray(), q);
      });
    },

    // via LET — remove-unnecessary-filters calls isTrue() and unlinks the filter
    testAllNoneEmptyArrayViaLetAlwaysTrue: function () {
      const opts = { optimizer: { rules: ['-all', '+remove-unnecessary-filters'] } };
      const queries = [
        `FOR doc IN ${cn} LET y = ([] ALL == doc.value) FILTER y RETURN doc`,
        `FOR doc IN ${cn} LET y = ([] NONE == doc.value) FILTER y RETURN doc`,
      ];
      queries.forEach(function(q) {
        const nodeTypes = helper.getCompactPlan(
          db._createStatement({query: q, bindVars: {}, options: opts}).explain()
        ).map(n => n.type);
        assertTrue(nodeTypes.indexOf('FilterNode') === -1, q);
        assertTrue(nodeTypes.indexOf('NoResultsNode') === -1, q);
        assertEqual(5, db._query(q, {}, opts).toArray().length, q);
      });
    },

    // [] ALL <op> x and [] NONE <op> x — vacuously true, filter removed
    testAllNoneEmptyArrayAlwaysTrue: function () {
      const queries = [
        `FOR doc IN ${cn} FILTER [] ALL == doc.value RETURN doc`,
        `FOR doc IN ${cn} FILTER [] ALL != doc.value RETURN doc`,
        `FOR doc IN ${cn} FILTER [] ALL > doc.value RETURN doc`,
        `FOR doc IN ${cn} FILTER [] ALL IN doc.value RETURN doc`,
        `FOR doc IN ${cn} FILTER [] ALL NOT IN doc.value RETURN doc`,
        `FOR doc IN ${cn} FILTER [] NONE == doc.value RETURN doc`,
        `FOR doc IN ${cn} FILTER [] NONE > doc.value RETURN doc`,
        `FOR doc IN ${cn} FILTER [] NONE IN doc.value RETURN doc`,
        `FOR doc IN ${cn} FILTER [] NONE NOT IN doc.value RETURN doc`,
      ];
      queries.forEach(function(q) {
        const nodeTypes = helper.getCompactPlan(
          db._createStatement({query: q, bindVars: {}, options: paramNoRules}).explain()
        ).map(n => n.type);
        assertTrue(nodeTypes.indexOf('FilterNode') === -1, q);
        assertTrue(nodeTypes.indexOf('NoResultsNode') === -1, q);
        assertEqual(5, db._query(q, {}, paramNoRules).toArray().length, q);
      });
    },

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(optimizerRuleTestSuite);
jsunity.run(emptyArrayFilterSuite);

return jsunity.done();

