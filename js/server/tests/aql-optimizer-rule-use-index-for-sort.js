/*jslint indent: 2, nomen: true, maxlen: 200, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports, assertTrue, assertEqual, AQL_EXECUTE, AQL_EXPLAIN, fail */

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
var errors = require("internal").errors;
var helper = require("org/arangodb/aql-helper");
var getQueryResults = helper.getQueryResults2;
var assertQueryError = helper.assertQueryError2;
var isEqual = helper.isEqual;
var findExecutionNodes = helper.findExecutionNodes;
var findReferencedNodes = helper.findReferencedNodes;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerRuleTestSuite() {
    var ruleName = "use-index-for-sort";
    var secondRuleName = "use-index-range";
    var colName = "UnitTestsAqlOptimizer" + ruleName.replace(/-/g, "_");
    
    var skiplist;
    var sortArray = function (l, r) {
              if (l[0] !== r[0]) {
                  return l[0] < r[0] ? -1 : 1;
              }
              if (l[1] !== r[1]) {
                  return l[1] < r[1] ? -1 : 1;
              }
              return 0;
          };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop(colName);
      skiplist = internal.db._create(colName);
      var i, j;
      for (j = 1; j <= 5; ++j) {
        for (i = 1; i <= 5; ++i) {
            skiplist.save({ "a" : i, "b": j , "c": j});
        }
        skiplist.save({ "a" : i, "c": j});
        skiplist.save({ "c": j});
      }

      skiplist.ensureSkiplist("a", "b");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      internal.db._drop(colName);
      skiplist = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has no effect when explicitly disabled
////////////////////////////////////////////////////////////////////////////////

    testRuleDisabled : function () {
/*
      var queries = [ 
        "LET a = 1 FOR i IN 1..10 FILTER a == 1 RETURN i",
        "FOR i IN 1..10 LET a = 25 RETURN i",
        "FOR i IN 1..10 LET a = 1 LET b = 2 LET c = 3 FILTER i > 3 LET d = 1 RETURN i"
      ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { }, { optimizer: { rules: [ "-all" ] } });
        assertEqual([ ], result.plan.rules);
      });
*/
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has no effect
////////////////////////////////////////////////////////////////////////////////
    testRuleNoEffect : function () {
/*          
      var queries = [ 
          "FOR v IN " + colName + " FILTER v.a == 1 SORT v.a RETURN v.a"
        "FOR i IN 1..10 FILTER i == 1 RETURN i",
        "FOR i IN 1..10 LET a = 25 + i RETURN i",
        "LET values = 1..10 FOR i IN values LET a = i + 1 FOR j IN values LET b = j + 2 RETURN i",
        "FOR i IN 1..10 FILTER i == 7 COLLECT x = i LET y = x RETURN y"
      ];

      queries.forEach(function(query) {
        require("internal").print(query);
        var result = AQL_EXPLAIN(query, { }, { optimizer: { rules: [ "-all", "+" + ruleName ] } });
        assertEqual([ ], result.plan.rules, query);
      });
*/
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has an effect
////////////////////////////////////////////////////////////////////////////////

    testRuleHasEffect : function () {
/*
      var queries = [ 
        "FOR i IN 1..10 LET a = 1 FILTER i == 1 RETURN i",
        "FOR i IN 1..10 LET a = 1 FILTER i == 1 RETURN a",
        "FOR i IN 1..10 FILTER i == 1 LET a = 1 RETURN i",
        "FOR i IN 1..10 LET a = 25 + 7 RETURN i",
        "FOR i IN 1..10 FOR j IN 1..10 LET a = i + 2 RETURN i",
        "FOR i IN 1..10 FILTER i == 7 LET a = i COLLECT x = i RETURN x"
      ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { }, { optimizer: { rules: [ "-all", "+" + ruleName ] } });
        assertEqual([ ruleName ], result.plan.rules);
      });
*/
    },

      testSortIndexable: function () {
/*
          var query = "FOR v IN " + colName + " SORT v.a RETURN [v.a, v.b]";

          var XPresult;
          var QResults=[];

          // we have to re-sort here, because of the index has one more sort criteria.
          QResults[0] = AQL_EXECUTE(query, { }, { optimizer: { rules: [ "-all" ] } }).json.sort(sortArray);

          // -> use-index-for-sort alone.
          XPresult    = AQL_EXPLAIN(query, { }, { optimizer: { rules: [ "-all"] } });

          XPresult    = AQL_EXPLAIN(query, { }, { optimizer: { rules: [ "-all", "+" + ruleName ] } });
          QResults[1] = AQL_EXECUTE(query, { }, { optimizer: { rules: [ "-all", "+" + ruleName ] } }).json;
          // our rule should have been applied.
          assertEqual([ ruleName ], XPresult.plan.rules);
          // The sortnode and its calculation node should have been removed.
          assertTrue(findExecutionNodes(XPresult, "SortNode").length === 0);
          assertTrue(findExecutionNodes(XPresult, "CalculationNode").length === 1);
          // The IndexRangeNode created by this rule is simple; it shouldn't have ranges.
          assertTrue(findExecutionNodes(XPresult, "IndexRangeNode").length === 1);
          assertTrue(findExecutionNodes(XPresult, "IndexRangeNode")[0].ranges.length === 0);

          assertTrue(isEqual(QResults[0], QResults[1]));
          */
      },

      testSortNonIndexable1: function () {
/*
          var query = "FOR v IN " + colName + " SORT v.c RETURN [v.a, v.b]";

          var XPresult;
          var QResults=[];

          // we have to re-sort here, because of the index has one more sort criteria.
          QResults[0] = AQL_EXECUTE(query, { }, { optimizer: { rules: [ "-all" ] } }).json;

          // -> use-index-for-sort - it shouldn't do anything.
          XPresult    = AQL_EXPLAIN(query, { }, { optimizer: { rules: [ "-all"] } });

          XPresult    = AQL_EXPLAIN(query, { }, { optimizer: { rules: [ "-all", "+" + ruleName ] } });
          QResults[1] = AQL_EXECUTE(query, { }, { optimizer: { rules: [ "-all", "+" + ruleName ] } }).json;
          require("internal").print(XPresult);
          // our rule should have been applied.
          assertEqual([ ruleName ], XPresult.plan.rules);
          // Nothing to optimize, everything should still be there.
          assertTrue(findExecutionNodes(XPresult, "SortNode").length === 1);
          assertTrue(findExecutionNodes(XPresult, "CalculationNode").length === 2);
          // There shouldn't be an IndexRangeNode
          assertTrue(findExecutionNodes(XPresult, "IndexRangeNode").length === 0);

          require("internal").print("equalz?");
          require("internal").print(          QResults[0]);
          require("internal").print(QResults[1].length);
          require("internal").print(          QResults[0].length);
          require("internal").print(QResults[1]);

          assertTrue(isEqual(QResults[0], QResults[1]));
        */  
      },

      testSortNonIndexable2: function () {
/*
          var query = "FOR v IN " + colName + " SORT v.b DESC RETURN [v.a, v.b]";

          var XPresult;
          var QResults=[];

          // we have to re-sort here, because of the index has one more sort criteria.
          QResults[0] = AQL_EXECUTE(query, { }, { optimizer: { rules: [ "-all" ] } }).json;

          // -> use-index-for-sort - it shouldn't do anything.
          XPresult    = AQL_EXPLAIN(query, { }, { optimizer: { rules: [ "-all", "+" + ruleName ] } });
          QResults[1] = AQL_EXECUTE(query, { }, { optimizer: { rules: [ "-all", "+" + ruleName ] } }).json;

          // our rule should have been applied.
          assertEqual([ ruleName ], XPresult.plan.rules);
          // Nothing to optimize, everything should still be there.
          assertTrue(findExecutionNodes(XPresult, "SortNode").length === 1);
          assertTrue(findExecutionNodes(XPresult, "CalculationNode").length === 2);
          // There shouldn't be an IndexRangeNode
          assertTrue(findExecutionNodes(XPresult, "IndexRangeNode").length === 0);

          assertTrue(isEqual(QResults[0], QResults[1]));
         */
      },

      testSortNonIndexable3: function () {
/*
          var query = "FOR v IN " + colName + " SORT v.c RETURN [v.a, v.b]";

          var XPresult;
          var QResults=[];

          // we have to re-sort here, because of the index has one more sort criteria.
          QResults[0] = AQL_EXECUTE(query, { }, { optimizer: { rules: [ "-all" ] } }).json;

          // -> use-index-for-sort - it shouldn't do anything.
          XPresult    = AQL_EXPLAIN(query, { }, { optimizer: { rules: [ "-all"] } });

          XPresult    = AQL_EXPLAIN(query, { }, { optimizer: { rules: [ "-all", "+" + ruleName ] } });
          QResults[1] = AQL_EXECUTE(query, { }, { optimizer: { rules: [ "-all", "+" + ruleName ] } }).json;
          require("internal").print(XPresult);
          // our rule should have been applied.
          assertEqual([ ruleName ], XPresult.plan.rules);
          // Nothing to optimize, everything should still be there.
          assertTrue(findExecutionNodes(XPresult, "SortNode").length === 1);
          assertTrue(findExecutionNodes(XPresult, "CalculationNode").length === 2);
          // There shouldn't be an IndexRangeNode
          assertTrue(findExecutionNodes(XPresult, "IndexRangeNode").length === 0);

          require("internal").print("equalz?");
          require("internal").print(          QResults[0]);
          require("internal").print(QResults[1].length);
          require("internal").print(          QResults[0].length);
          require("internal").print(QResults[1]);

          assertTrue(isEqual(QResults[0], QResults[1]));
*/
      },


      testSortMoreThanIndexed: function () {

          var query = "FOR v IN " + colName + " FILTER v.a == 1 SORT v.a, v.c RETURN [v.a, v.b, v.c]";
          // no index can be used for v.c -> sort has to remain in place!
          var XPresult;
          var QResults=[];
          var i;

          // the index we will compare to sorts by a & b, so we need to re-sort the result here to accomplish similarity.
          QResults[0] = AQL_EXECUTE(query, { }, { optimizer: { rules: [ "-all" ] } }).json.sort(sortArray);

          // -> use-index-for-sort alone.
          QResults[1] = AQL_EXECUTE(query, { }, { optimizer: { rules: [ "-all", "+" + ruleName ] } }).json;
          XPresult    = AQL_EXPLAIN(query, { }, { optimizer: { rules: [ "-all", "+" + ruleName ] } });
          // our rule should be there.
          assertEqual([ ruleName ], XPresult.plan.rules);
          // The sortnode and its calculation node should have been removed.
          assertTrue(findExecutionNodes(XPresult, "SortNode").length === 1);
          assertTrue(findExecutionNodes(XPresult, "CalculationNode").length === 4);
          // The IndexRangeNode created by this rule is simple; it shouldn't have ranges.
          assertTrue(findExecutionNodes(XPresult, "IndexRangeNode").length === 1);
          assertTrue(findExecutionNodes(XPresult, "IndexRangeNode")[0].ranges.length === 0);

          // -> combined use-index-for-sort and use-index-range
          QResults[2] = AQL_EXECUTE(query, { }, { optimizer: { rules: [ "-all", "+" + ruleName, "+" + secondRuleName ] } }).json;
          XPresult    = AQL_EXPLAIN(query, { }, { optimizer: { rules: [ "-all", "+" + ruleName, "+" + secondRuleName ] } });
          assertEqual([ secondRuleName, ruleName ].sort(), XPresult.plan.rules.sort());
          // The sortnode and its calculation node should have been removed.
          assertTrue(findExecutionNodes(XPresult, "SortNode").length === 1);
          assertTrue(findExecutionNodes(XPresult, "CalculationNode").length === 4);
          // The IndexRangeNode created by this rule should be more clever, it knows the ranges.
          assertTrue(findExecutionNodes(XPresult, "IndexRangeNode").length === 1);
          assertTrue(findExecutionNodes(XPresult, "IndexRangeNode")[0].ranges.length > 0);

          // -> use-index-range alone.
          QResults[3] = AQL_EXECUTE(query, { }, { optimizer: { rules: [ "-all", "+" + secondRuleName ] } }).json;
          XPresult    = AQL_EXPLAIN(query, { }, { optimizer: { rules: [ "-all", "+" + secondRuleName ] } });
          assertEqual([ secondRuleName ], XPresult.plan.rules);
          // the sortnode and its calculation node should be there. 

          assertTrue(findExecutionNodes(XPresult, "SortNode").length === 1);
          assertTrue(findExecutionNodes(XPresult, "CalculationNode").length === 4);
          // we should be able to find exactly one sortnode property - its a Calculation node.
          var sortProperty = findReferencedNodes(XPresult, findExecutionNodes(XPresult, "SortNode")[0]);
          assertTrue(sortProperty.length === 2);
          assertTrue(sortProperty[0].type === "CalculationNode");
          assertTrue(sortProperty[1].type === "CalculationNode");
          // The IndexRangeNode created by this rule should be more clever, it knows the ranges.
          require("internal").print(findExecutionNodes(XPresult, "IndexRangeNode"));
          require("internal").print(findExecutionNodes(XPresult, "IndexRangeNode").length);
          assertEqual(1, findExecutionNodes(XPresult, "IndexRangeNode").length);

          require("internal").print(findExecutionNodes(XPresult, "IndexRangeNode")[0].ranges);
          assertTrue(findExecutionNodes(XPresult, "IndexRangeNode")[0].ranges.length > 0);

          for (i = 1; i < 4; i++) {
              assertTrue(isEqual(QResults[0], QResults[i]));
          }
          
      },
      testRangeSuperseedsSort: function () {
/*
          var query = "FOR v IN " + colName + " FILTER v.a == 1 SORT v.a RETURN [v.a, v.b, v.c]";

          var XPresult;
          var QResults=[];
          var i;

          // the index we will compare to sorts by a & b, so we need to re-sort the result here to accomplish similarity.
          QResults[0] = AQL_EXECUTE(query, { }, { optimizer: { rules: [ "-all" ] } }).json.sort(sortArray);

          // -> use-index-for-sort alone.
          QResults[1] = AQL_EXECUTE(query, { }, { optimizer: { rules: [ "-all", "+" + ruleName ] } }).json;
          XPresult    = AQL_EXPLAIN(query, { }, { optimizer: { rules: [ "-all", "+" + ruleName ] } });
          // our rule should be there.
          assertEqual([ ruleName ], XPresult.plan.rules);
          // The sortnode and its calculation node should have been removed.
          assertTrue(findExecutionNodes(XPresult, "SortNode").length === 0);
          assertTrue(findExecutionNodes(XPresult, "CalculationNode").length === 2);
          // The IndexRangeNode created by this rule is simple; it shouldn't have ranges.
          assertTrue(findExecutionNodes(XPresult, "IndexRangeNode").length === 1);
          assertTrue(findExecutionNodes(XPresult, "IndexRangeNode")[0].ranges.length === 0);

          // -> combined use-index-for-sort and use-index-range
          QResults[2] = AQL_EXECUTE(query, { }, { optimizer: { rules: [ "-all", "+" + ruleName, "+" + secondRuleName ] } }).json;
          XPresult    = AQL_EXPLAIN(query, { }, { optimizer: { rules: [ "-all", "+" + ruleName, "+" + secondRuleName ] } });
          assertEqual([ secondRuleName, ruleName ].sort(), XPresult.plan.rules.sort());
          // The sortnode and its calculation node should have been removed.
          assertTrue(findExecutionNodes(XPresult, "SortNode").length === 0);
          assertTrue(findExecutionNodes(XPresult, "CalculationNode").length === 2);
          // The IndexRangeNode created by this rule should be more clever, it knows the ranges.
          assertTrue(findExecutionNodes(XPresult, "IndexRangeNode").length === 1);
          assertTrue(findExecutionNodes(XPresult, "IndexRangeNode")[0].ranges.length > 0);

          // -> use-index-range alone.
          QResults[3] = AQL_EXECUTE(query, { }, { optimizer: { rules: [ "-all", "+" + secondRuleName ] } }).json;
          XPresult    = AQL_EXPLAIN(query, { }, { optimizer: { rules: [ "-all", "+" + secondRuleName ] } });
          assertEqual([ secondRuleName ], XPresult.plan.rules);
          // the sortnode and its calculation node should be there.
          assertTrue(findExecutionNodes(XPresult, "SortNode").length === 1);
          assertTrue(findExecutionNodes(XPresult, "CalculationNode").length === 3);
          // we should be able to find exactly one sortnode property - its a Calculation node.
          var sortProperty = findReferencedNodes(XPresult, findExecutionNodes(XPresult, "SortNode")[0]);
          assertTrue(sortProperty.length === 1);
          assertTrue(sortProperty[0].type === "CalculationNode");
          // The IndexRangeNode created by this rule should be more clever, it knows the ranges.
          require("internal").print(findExecutionNodes(XPresult, "IndexRangeNode"));
          require("internal").print(findExecutionNodes(XPresult, "IndexRangeNode").length);
          assertEqual(1, findExecutionNodes(XPresult, "IndexRangeNode").length);

          require("internal").print(findExecutionNodes(XPresult, "IndexRangeNode")[0].ranges);
          assertTrue(findExecutionNodes(XPresult, "IndexRangeNode")[0].ranges.length > 0);

          for (i = 1; i < 4; i++) {
              assertTrue(isEqual(QResults[0], QResults[i]));
          }
          */
      },

////////////////////////////////////////////////////////////////////////////////
/// @brief test generated plans
////////////////////////////////////////////////////////////////////////////////

    testPlans : function () {
/*
      var plans = [ 
        [ "FOR i IN 1..10 LET a = 1 RETURN i", [ "SingletonNode", "CalculationNode", "CalculationNode", "EnumerateListNode", "ReturnNode" ] ],
        [ "FOR i IN 1..10 FILTER i == 1 LET a = 1 RETURN i", [ "SingletonNode", "CalculationNode", "CalculationNode", "EnumerateListNode", "CalculationNode", "FilterNode", "ReturnNode"  ] ]
      ];

      plans.forEach(function(plan) {
        var result = AQL_EXPLAIN(plan[0], { }, { optimizer: { rules: [ "-all", "+" + ruleName ] } });
        assertEqual([ ruleName ], result.plan.rules, plan[0]);
        assertEqual(plan[1], helper.getCompactPlan(result).map(function(node) { return node.type; }), plan[0]);
      });
*/
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test results
////////////////////////////////////////////////////////////////////////////////

    testResults : function () {
/*
      var queries = [ 
        [ "FOR i IN 1..10 LET a = 1 RETURN i", [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] ],
        [ "FOR i IN 1..10 LET a = 1 RETURN a", [ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 ] ],
        [ "FOR i IN 1..10 FILTER i == 1 LET a = 1 RETURN i", [ 1 ] ],
        [ "FOR i IN 1..10 FILTER i == 1 LET a = 1 RETURN a", [ 1 ] ],
      ];

      queries.forEach(function(query) {
        var planDisabled   = AQL_EXPLAIN(query[0], { }, { optimizer: { rules: [ "+all", "-" + ruleName ] } });
        var planEnabled    = AQL_EXPLAIN(query[0], { }, { optimizer: { rules: [ "-all", "+" + ruleName ] } });

        var resultDisabled = AQL_EXECUTE(query[0], { }, { optimizer: { rules: [ "+all", "-" + ruleName ] } });
        var resultEnabled  = AQL_EXECUTE(query[0], { }, { optimizer: { rules: [ "-all", "+" + ruleName ] } });

        assertTrue(planDisabled.plan.rules.indexOf(ruleName) === -1, query[0]);
        assertTrue(planEnabled.plan.rules.indexOf(ruleName) !== -1, query[0]);

        assertEqual(resultDisabled.json, query[1], query[0]);
        assertEqual(resultEnabled.json, query[1], query[0]);
      });
*/
    }


  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(optimizerRuleTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
