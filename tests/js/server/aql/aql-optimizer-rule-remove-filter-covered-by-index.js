/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, assertTrue, AQL_EXPLAIN, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for optimizer rules
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal");
var jsunity = require("jsunity");
var helper = require("@arangodb/aql-helper");
var isEqual = helper.isEqual;
var findExecutionNodes = helper.findExecutionNodes;
var getQueryMultiplePlansAndExecutions = helper.getQueryMultiplePlansAndExecutions;
var removeAlwaysOnClusterRules = helper.removeAlwaysOnClusterRules;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerRuleTestSuite() {
  var IndexesRule = "use-indexes";
  var FilterRemoveRule = "remove-filter-covered-by-index";
  var SortRemoveRule = "use-index-for-sort"; 
  var colName = "UnitTestsAqlOptimizer" + FilterRemoveRule.replace(/-/g, "_");
  var colNameOther = colName + "_XX";

  // various choices to control the optimizer: 
  var paramNone = { optimizer: { rules: [ "-all" ] } };
  var paramIndexRangeFilter = { optimizer: { rules: [ "-all", "+" + IndexesRule,  "+" + FilterRemoveRule] } };
  var paramIndexRangeSortFilter = { optimizer: { rules: [ "-all", "+" + IndexesRule,  "+" + FilterRemoveRule, "+" + SortRemoveRule] } };

  var hasNoFilterNode = function (plan) {
    assertEqual(findExecutionNodes(plan, "FilterNode").length, 0, "has a filter node");
  };
  
  var hasFilterNode = function (plan) {
    assertNotEqual(findExecutionNodes(plan, "FilterNode").length, 0, "has no filter node");
  };
  
  var hasIndexNodeWithRanges = function (plan) {
    var rn = findExecutionNodes(plan, "IndexNode");
    assertTrue(rn.length >= 1, "has IndexNode");
    assertTrue(rn[0].indexes.length > 0, "whether the IndexNode uses at least one index");
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
/// Datastructure: 
///  - double index on (a,b)/(f,g) for tests with these
///  - single column index on d/j to test sort behavior without sub-columns
///  - non-indexed columns c/h to sort without indices.
///  - non-skiplist indexed columns e/j to check whether its not selecting them.
///  - join column 'joinme' to intersect both tables.
////////////////////////////////////////////////////////////////////////////////

    setUpAll : function () {
      const loopTo = 10;

      internal.db._drop(colName);
      let collection = internal.db._create(colName, { numberOfShards: 2 });
      let docs = [];
      for (let j = 1; j <= loopTo; ++j) {
        for (let i = 1; i <= loopTo; ++i) {
          docs.push({ "a" : i, "b": j , "c": j, "d": i, "e": i, "joinme" : "aoeu " + j });
        }
        docs.push({ "a" : loopTo, "c": j, "d": loopTo, "e": loopTo, "joinme" : "aoeu " + j });
        docs.push({ "c": j, "joinme" : "aoeu " + j });
      }
      collection.insert(docs);

      collection.ensureIndex({ type: "persistent", fields: ["a", "b"] });
      collection.ensureIndex({ type: "persistent", fields: ["d"] });
      collection.ensureIndex({ type: "persistent", fields: ["z[*]"] });
      collection.ensureIndex({ type: "persistent", fields: [ "c" ], unique: false });

      internal.db._drop(colNameOther);
      collection = internal.db._create(colNameOther);
      docs = [];
      for (let j = 1; j <= loopTo; ++j) {
        for (let i = 1; i <= loopTo; ++i) {
          docs.push({ "f" : i, "g": j , "h": j, "i": i, "j": i, "joinme" : "aoeu " + j });
        }
        docs.push({ "f" : loopTo, "g": j, "i": loopTo, "j": loopTo, "joinme" : "aoeu " + j });
        docs.push({ "h": j, "joinme" : "aoeu " + j });
      }
      collection.insert(docs);

      collection.ensureIndex({ type: "persistent", fields: ["f", "g"] });
      collection.ensureIndex({ type: "persistent", fields: ["i"] });
      collection.ensureIndex({ type: "persistent", fields: [ "h" ], unique: false });
      collection.ensureIndex({ type: "persistent", fields: [ "x", "y" ], unique: false });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDownAll : function () {
      internal.db._drop(colName);
      internal.db._drop(colNameOther);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has no effect
////////////////////////////////////////////////////////////////////////////////
/*
    testRuleNoEffect : function () {
      var j;
      var queries = [ 

        ["FOR v IN " + colName + " FILTER ODD(v.c) RETURN [v.a, v.b]", true],
      ];//// todo: mehr davon

      queries.forEach(function(query) {
        
        var result = AQL_EXPLAIN(query[0], { }, paramIndexFromSort);
        assertEqual([], removeAlwaysOnClusterRules(result.plan.rules), query);
        if (query[1]) {
          var allresults = getQueryMultiplePlansAndExecutions(query[0], {});
          for (j = 1; j < allresults.results.length; j++) {
            assertTrue(isEqual(allresults.results[0],
                               allresults.results[j]),
                       "whether the execution of '" + query[0] +
                       "' this plan gave the wrong results: " + JSON.stringify(allresults.plans[j]) +
                       " Should be: '" + JSON.stringify(allresults.results[0]) +
                       "' but Is: " + JSON.stringify(allresults.results[j]) + "'"
                      );
          }
        }
      });

    },
*/

////////////////////////////////////////////////////////////////////////////////
/// @brief test with non-scalar values
////////////////////////////////////////////////////////////////////////////////

    testRuleWithNonScalarValues : function () {
      var queries = [ 
        "FOR v IN " + colName + " FILTER v.a == [ ] RETURN 1",
        "FOR v IN " + colName + " FILTER v.a == [ 1, 2, 3 ] RETURN 1",
        "FOR v IN " + colName + " FILTER v.a == [ ] && v.b == [ ] RETURN 1",
        "FOR v IN " + colName + " FILTER v.a == { } RETURN 1",
        "FOR v IN " + colName + " FILTER v.a == { a: 1, b: 2 } RETURN 1",
        "FOR v IN " + colName + " FILTER v.a == { } && v.b == { } RETURN 1",
        "FOR v IN " + colName + " FILTER [ ] == v.a RETURN 1",
        "FOR v IN " + colName + " FILTER [ 1, 2, 3 ] == v.a RETURN 1",
        "FOR v IN " + colName + " FILTER [ ] == v.a && [ ] == v.b RETURN 1",
        "FOR v IN " + colName + " FILTER { } == v.a RETURN 1",
        "FOR v IN " + colName + " FILTER { a: 1, b: 2 } == v.a RETURN 1",
        "FOR v IN " + colName + " FILTER { } == v.a && { } == v.b RETURN 1"
      ];

      queries.forEach(function(query) {
        var result;
        result = AQL_EXPLAIN(query, { }, paramIndexRangeFilter);
        assertEqual([ IndexesRule, FilterRemoveRule ], removeAlwaysOnClusterRules(result.plan.rules), query);
        hasNoFilterNode(result);

        hasIndexNodeWithRanges(result);

        result = AQL_EXPLAIN(query, { }, paramIndexRangeSortFilter);
        assertEqual([ IndexesRule, FilterRemoveRule ], removeAlwaysOnClusterRules(result.plan.rules), query);
        hasNoFilterNode(result);
        hasIndexNodeWithRanges(result);

        var QResults = [];
        QResults[0] = AQL_EXECUTE(query, { }, paramNone).json;
        QResults[1] = AQL_EXECUTE(query, { }, paramIndexRangeFilter).json;
        QResults[2] = AQL_EXECUTE(query, { }, paramIndexRangeSortFilter).json;

        assertTrue(isEqual(QResults[0], QResults[1]), "result is equal?");
        assertTrue(isEqual(QResults[0], QResults[2]), "result is equal?");

        var allResults = getQueryMultiplePlansAndExecutions(query, {});
        for (var j = 1; j < allResults.results.length; j++) {
            assertTrue(isEqual(allResults.results[0],
                               allResults.results[j]),
                       "while executing '" + query +
                       "' this plan gave the wrong results: " + JSON.stringify(allResults.plans[j]) +
                       " Should be: '" + JSON.stringify(allResults.results[0]) +
                       "', but is: " + JSON.stringify(allResults.results[j]) + "'"
                      );
        }
      });

    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has an effect for multiple conditions
////////////////////////////////////////////////////////////////////////////////

    testMultipleConditions : function () {
      var query = "FOR v IN " + colName + " FILTER v.d == 'foo' || v.d == 'bar' RETURN v";
      var result = AQL_EXPLAIN(query);
      assertEqual([ "remove-filter-covered-by-index", "remove-unnecessary-calculations-2", "use-indexes", "replace-or-with-in" ].sort(),  
        removeAlwaysOnClusterRules(result.plan.rules.sort()), query);
      hasNoFilterNode(result);
      hasIndexNodeWithRanges(result);
      
      query = "FOR v IN " + colName + " FILTER 'foo' IN v.z && 'bar' IN v.z RETURN v";
      result = AQL_EXPLAIN(query);
      // should optimize away one part of the filter
      assertEqual([ "remove-filter-covered-by-index", "use-indexes", "move-filters-into-enumerate" ].sort(),  
        removeAlwaysOnClusterRules(result.plan.rules.sort()), query);
      hasIndexNodeWithRanges(result);
      
      result = AQL_EXPLAIN(query, null, { optimizer: { rules: ["-move-filters-into-enumerate"] } });
      // should optimize away one part of the filter
      assertEqual([ "remove-filter-covered-by-index", "use-indexes" ].sort(),  
        removeAlwaysOnClusterRules(result.plan.rules.sort()), query);
      hasFilterNode(result);
      hasIndexNodeWithRanges(result);
      
      query = "FOR v IN " + colName + " FILTER 'foo' IN v.z[*] && 'bar' IN v.z[*] RETURN v";
      result = AQL_EXPLAIN(query);
      // should optimize away one part of the filter
      assertEqual([ "remove-filter-covered-by-index", "use-indexes", "move-filters-into-enumerate" ].sort(),  
        removeAlwaysOnClusterRules(result.plan.rules.sort()), query);
      hasIndexNodeWithRanges(result);
      
      result = AQL_EXPLAIN(query, null, { optimizer: { rules: ["-move-filters-into-enumerate"] } });
      // should optimize away one part of the filter
      assertEqual([ "remove-filter-covered-by-index", "use-indexes" ].sort(),  
        removeAlwaysOnClusterRules(result.plan.rules.sort()), query);
      hasFilterNode(result);
      hasIndexNodeWithRanges(result);
      
      query = "FOR v IN " + colName + " FILTER 'foo' IN v.z || 'bar' IN v.z RETURN v";
      result = AQL_EXPLAIN(query);
      assertEqual([ "use-indexes" ].sort(),  
        removeAlwaysOnClusterRules(result.plan.rules.sort()), query);
      hasFilterNode(result);
      hasIndexNodeWithRanges(result);
      
      query = "FOR v IN " + colName + " FILTER 'foo' IN v.z[*] || 'bar' IN v.z[*] RETURN v";
      result = AQL_EXPLAIN(query);
      assertEqual([ "use-indexes" ].sort(),  
        removeAlwaysOnClusterRules(result.plan.rules.sort()), query);
      hasFilterNode(result);
      hasIndexNodeWithRanges(result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has an effect on the FILTER and the SORT
////////////////////////////////////////////////////////////////////////////////

    testRuleHasEffectCombineSortFilter : function () {
      var queries = [ 
        "FOR v IN " + colName + " FILTER v.a > 5 SORT v.a ASC RETURN [v.a]",
        "FOR v IN " + colName + " SORT v.a ASC FILTER v.a > 5 RETURN [v.a]",
        "FOR v IN " + colName + " FILTER v.d > 5 SORT v.d ASC RETURN [v.d]",
        "FOR v IN " + colName + " SORT v.d ASC FILTER v.d > 5 RETURN [v.d]",
        "FOR v IN " + colName + " SORT v.d FILTER v.d > 2 LIMIT 3 RETURN [v.d]",
        "FOR v IN " + colName + " FILTER v.d > 2 SORT v.d LIMIT 3 RETURN [v.d]",
        "FOR v IN " + colName + " LET x = (FOR w IN " + colNameOther + " RETURN w.f) FILTER v.a > 4 SORT v.a RETURN [v.a]",
        "FOR v IN " + colName + " LET x = (FOR w IN " + colNameOther + " RETURN w.f) SORT v.a FILTER v.a > 4 RETURN [v.a]"
      ];

      queries.forEach(function(query) {
        var result;
        result = AQL_EXPLAIN(query, { }, paramIndexRangeFilter);
        assertEqual([ IndexesRule, FilterRemoveRule ], 
          removeAlwaysOnClusterRules(result.plan.rules).filter((r) => r !== "splice-subqueries"), query);
        hasNoFilterNode(result);

        hasIndexNodeWithRanges(result);

        result = AQL_EXPLAIN(query, { }, paramIndexRangeSortFilter);
        assertEqual([ IndexesRule, FilterRemoveRule, SortRemoveRule ], 
          removeAlwaysOnClusterRules(result.plan.rules).filter((r) => r !== "splice-subqueries"), query);
        hasNoFilterNode(result);
        hasIndexNodeWithRanges(result);

        var QResults = [];
        QResults[0] = AQL_EXECUTE(query, { }, paramNone).json;
        QResults[1] = AQL_EXECUTE(query, { }, paramIndexRangeFilter).json;
        QResults[2] = AQL_EXECUTE(query, { }, paramIndexRangeSortFilter).json;

        assertTrue(isEqual(QResults[0], QResults[1]), "result is equal?");
        assertTrue(isEqual(QResults[0], QResults[2]), "result is equal?");

        var allResults = getQueryMultiplePlansAndExecutions(query, {});
        for (var j = 1; j < allResults.results.length; j++) {
            assertTrue(isEqual(allResults.results[0],
                               allResults.results[j]),
                       "while executing '" + query +
                       "' this plan gave the wrong results: " + JSON.stringify(allResults.plans[j]) +
                       " Should be: '" + JSON.stringify(allResults.results[0]) +
                       "', but is: " + JSON.stringify(allResults.results[j]) + "'"
                      );
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has an effect on the FILTER and the SORT
////////////////////////////////////////////////////////////////////////////////

    testRuleHasEffectCombineSortFilter2 : function () {
      var queries = [ 
        "FOR v IN " + colName + " FOR w IN 1..10 FILTER v.d > 2 SORT v.d RETURN [v.d]",
        "FOR v IN " + colName + " FOR w IN 1..10 SORT v.d FILTER v.d > 2 RETURN [v.d]",
      ];

      queries.forEach(function(query) {
        var result;
        result = AQL_EXPLAIN(query, { }, paramIndexRangeFilter);
        assertEqual([ IndexesRule, FilterRemoveRule ], 
          removeAlwaysOnClusterRules(result.plan.rules), query);
        hasNoFilterNode(result);

        hasIndexNodeWithRanges(result);

        result = AQL_EXPLAIN(query, { }, paramIndexRangeSortFilter);
        assertEqual([ IndexesRule, FilterRemoveRule ], removeAlwaysOnClusterRules(result.plan.rules), query);
        hasNoFilterNode(result);
        hasIndexNodeWithRanges(result);

        var QResults = [];
        QResults[0] = AQL_EXECUTE(query, { }, paramNone).json;
        QResults[1] = AQL_EXECUTE(query, { }, paramIndexRangeFilter).json;
        QResults[2] = AQL_EXECUTE(query, { }, paramIndexRangeSortFilter).json;

        assertTrue(isEqual(QResults[0], QResults[1]), "result is equal?");
        assertTrue(isEqual(QResults[0], QResults[2]), "result is equal?");

        var allResults = getQueryMultiplePlansAndExecutions(query, {});
        for (var j = 1; j < allResults.results.length; j++) {
            assertTrue(isEqual(allResults.results[0],
                               allResults.results[j]),
                       "while executing '" + query +
                       "' this plan gave the wrong results: " + JSON.stringify(allResults.plans[j]) +
                       " Should be: '" + JSON.stringify(allResults.results[0]) +
                       "', but is: " + JSON.stringify(allResults.results[j]) + "'"
                      );
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has an effect, the filter can be removed, but the sort
/// is kept in place since the index can't fullfill all the sorting.
////////////////////////////////////////////////////////////////////////////////

    testRuleHasEffectCombineSortNoFilter : function () {
      var queries = [ 
        "FOR v IN " + colName + " FILTER v.a == 1 SORT v.a, v.c RETURN [v.a, v.b, v.c]",
        "FOR v IN " + colName + " LET x = (FOR w IN " 
          + colNameOther + " FILTER w.f == 1 SORT w.a, w.h RETURN  w.f ) SORT v.a , v.c FILTER v.a == 1 RETURN [v.a, x]"
      ];
      queries.forEach(function(query) {
        var result;

        result = AQL_EXPLAIN(query, { }, paramIndexRangeFilter);
        assertEqual([ IndexesRule, FilterRemoveRule ], 
          removeAlwaysOnClusterRules(result.plan.rules).filter((r) => r !== "splice-subqueries"), query);
        hasNoFilterNode(result);
        hasIndexNodeWithRanges(result);

        result = AQL_EXPLAIN(query, { }, paramIndexRangeSortFilter);
        var rules = removeAlwaysOnClusterRules(result.plan.rules);
        assertNotEqual(-1, rules.indexOf(IndexesRule)); 
        assertNotEqual(-1, rules.indexOf(FilterRemoveRule)); 
        hasNoFilterNode(result);
        hasIndexNodeWithRanges(result);

        var QResults = [];
        QResults[0] = AQL_EXECUTE(query, { }, paramNone).json;
        QResults[1] = AQL_EXECUTE(query, { }, paramIndexRangeFilter).json;
        QResults[2] = AQL_EXECUTE(query, { }, paramIndexRangeSortFilter).json;

        assertTrue(isEqual(QResults[0], QResults[1]), "result is equal?");
        assertTrue(isEqual(QResults[0], QResults[2]), "result is equal?");

        var allResults = getQueryMultiplePlansAndExecutions(query, {});
        for (var j = 1; j < allResults.results.length; j++) {
          assertTrue(isEqual(allResults.results[0],
                             allResults.results[j]),
                     "while executing '" + query +
                     "' this plan gave the wrong results: " + JSON.stringify(allResults.plans[j]) +
                     " Should be: '" + JSON.stringify(allResults.results[0]) +
                     "', but is: " + JSON.stringify(allResults.results[j]) + "'"
                    );
        }
      });
    },

    testNoOptimizeAwayUsedVariables : function() {
      var queries = [ 
        "FOR v IN " + colName + " LET cond1 = v.a == 1 FILTER cond1 RETURN v",
        "FOR v IN " + colName + " LET cond1 = v.a == 1 FILTER cond1 RETURN { v, cond1 }",
        "FOR v IN " + colName + " LET cond1 = v.a == 1 LET cond2 = v.b == 1 FILTER cond1 FILTER cond2 RETURN v",
        "FOR v IN " + colName + " LET cond1 = v.a == 1 LET cond2 = v.b == 1 FILTER cond1 FILTER cond2 RETURN { v, cond1, cond2 }"
      ];
      queries.forEach(function(query) {
        var result;

        result = AQL_EXPLAIN(query, { }, paramIndexRangeFilter);
        assertEqual([ IndexesRule, FilterRemoveRule ], 
          removeAlwaysOnClusterRules(result.plan.rules), query);
        hasNoFilterNode(result);
        hasIndexNodeWithRanges(result);

        result = AQL_EXPLAIN(query, { }, paramIndexRangeSortFilter);
        assertEqual([ IndexesRule, FilterRemoveRule ], 
          removeAlwaysOnClusterRules(result.plan.rules), query);
        hasNoFilterNode(result);
        hasIndexNodeWithRanges(result);
      });
    },

    testNoOptimizeAwayUsedVariablesOr : function() {
      var queries = [ 
        "FOR v IN " + colName + " LET cond1 = v.a == 1 LET cond2 = v.d == 1 FILTER cond1 || cond2 RETURN v",
        "FOR v IN " + colName + " LET cond1 = v.a == 1 LET cond2 = v.d == 1 FILTER cond1 || cond2 RETURN { v, cond1, cond2 }"
      ];
      queries.forEach(function(query) {
        var result;

        result = AQL_EXPLAIN(query, { }, paramIndexRangeFilter);
        assertEqual([ IndexesRule ], 
          removeAlwaysOnClusterRules(result.plan.rules), query);
        hasIndexNodeWithRanges(result);

        result = AQL_EXPLAIN(query, { }, paramIndexRangeSortFilter);
        assertEqual([ IndexesRule ], 
          removeAlwaysOnClusterRules(result.plan.rules), query);
        hasIndexNodeWithRanges(result);
      });
    },

    testOptimizeAwayFilter : function() {
      var query = "FOR outer IN [ { id: 123 } ] LET id = outer.id RETURN (FOR inner IN "  + colNameOther + " FILTER inner.x == id && inner.y == null RETURN inner)";
      
      var result = AQL_EXPLAIN(query, { }, paramIndexRangeFilter);
      assertEqual([ FilterRemoveRule, IndexesRule ].sort(), 
        removeAlwaysOnClusterRules(result.plan.rules).sort().filter((r) => r !== "splice-subqueries"), query);
      hasIndexNodeWithRanges(result);
      hasNoFilterNode(result);

      result = AQL_EXPLAIN(query, { }, paramIndexRangeSortFilter);
      assertEqual([ FilterRemoveRule, IndexesRule ].sort(), 
        removeAlwaysOnClusterRules(result.plan.rules).sort().filter((r) => r !== "splice-subqueries"), query);
      hasIndexNodeWithRanges(result);
      hasNoFilterNode(result);
    },

    testOptimizeAwayFilterWhenAliased : function() {
      let query = "FOR doc IN "  + colNameOther + " LET xAlias = doc.x FILTER xAlias == 3 RETURN doc";
      let result = AQL_EXPLAIN(query);
      let rules = removeAlwaysOnClusterRules(result.plan.rules);
      assertNotEqual(-1, rules.indexOf(IndexesRule));
      assertNotEqual(-1, rules.indexOf(FilterRemoveRule));
      assertEqual(0, result.plan.nodes.filter(function(node) { return node.type === 'CalculationNode'; }).length);
      hasIndexNodeWithRanges(result);
      hasNoFilterNode(result);
    },
    
    testOptimizeAwayFiltersWhenAliased : function() {
      let query = "FOR doc IN "  + colNameOther + " LET xAlias = doc.x FILTER xAlias > 3 FILTER xAlias < 6 RETURN doc";
      let result = AQL_EXPLAIN(query);
      let rules = removeAlwaysOnClusterRules(result.plan.rules);
      assertNotEqual(-1, rules.indexOf(IndexesRule));
      assertNotEqual(-1, rules.indexOf(FilterRemoveRule));
      assertEqual(0, result.plan.nodes.filter(function(node) { return node.type === 'CalculationNode'; }).length);
      hasIndexNodeWithRanges(result);
      hasNoFilterNode(result);
    }

  };
}

jsunity.run(optimizerRuleTestSuite);

return jsunity.done();
