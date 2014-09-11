/*jslint indent: 2, nomen: true, maxlen: 200, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports, assertTrue, assertEqual, AQL_EXECUTE, AQL_EXECUTEJSON, AQL_EXPLAIN, fail, loopmax */

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
var PY = function (plan) { require("internal").print(require("js-yaml").safeDump(plan));};
var internal = require("internal");
var jsunity = require("jsunity");
var errors = require("internal").errors;
var helper = require("org/arangodb/aql-helper");
var getQueryResults = helper.getQueryResults2;
var assertQueryError = helper.assertQueryError2;
var isEqual = helper.isEqual;
var findExecutionNodes = helper.findExecutionNodes;
var findReferencedNodes = helper.findReferencedNodes;
//var getQueryMultiplePlansAndExecutions = helper.getQueryMultiplePlansAndExecutions;

function getQueryMultiplePlansAndExecutions (query, bindVars) {
    var plan;
    var i;
    var plans = [];
    var allPlans = [];
    var results = [];
    var paramNone     = { optimizer: { rules: [ "-all" ]},  verbosePlans: true};
    var paramAllPlans = { allPlans : true, verbosePlans: true};
    PY(query);
    // first fetch the unmodified version
    plans [0] = AQL_EXPLAIN(query, bindVars, paramNone);
    // then all of the ones permuted by by the optimizer.
    allPlans = AQL_EXPLAIN(query, bindVars, paramAllPlans);

    PY(allPlans.plans[0]);

    for (i=0; i < allPlans.plans.length; i++) {
        plans[i+1] = {'plan':allPlans.plans[i]};
    }
    // Now execute each of these variations.
    for (i=0; i < plans.length; i++) {
        PY(plans[i]);
        results += AQL_EXECUTEJSON(plans[i].plan, {});
    }

    return {'plans': plans, 'results': results};
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerRuleTestSuite() {
    var ruleName = "use-index-for-sort";
    var secondRuleName = "use-index-range";
    var removeCalculationNodes = "remove-unnecessary-calculations-2";
    var thirdRuleName = "remove-redundant-sorts";
    var colName = "UnitTestsAqlOptimizer" + ruleName.replace(/-/g, "_");
    var colNameOther = colName + "_XX";

    // various choices to control the optimizer: 
    var paramNone = { optimizer: { rules: [ "-all" ] } };
    var paramIndexFromSort  = { optimizer: { rules: [ "-all", "+" + ruleName ] } };
    var paramIndexRange   = { optimizer: { rules: [ "-all", "+" + secondRuleName ] } };
    var paramRedundantSort   = { optimizer: { rules: [ "-all", "+" + thirdRuleName ] } };
    var paramIndexFromSort_IndexRange = { optimizer: { rules: [ "-all", "+" + ruleName, "+" + secondRuleName ] } };
    var paramIndexRangeRemoveCalculations   = {
        optimizer: { rules: [ "-all", "+" + secondRuleName, "+" + removeCalculationNodes ] }
    };
    var paramIndexFromSort_IndexRange_RemoveCalculations = {
        optimizer: { rules: [ "-all", "+" + ruleName, "+" + secondRuleName, "+" + removeCalculationNodes ] }
    };
    var paramIndexFromSort_RemoveCalculations = {
        optimizer: { rules: [ "-all", "+" + ruleName, "+" + removeCalculationNodes ] }
    };

    var skiplist;
    var skiplist2;
    var sortArray = function (l, r) {
              if (l[0] !== r[0]) {
                  return l[0] < r[0] ? -1 : 1;
              }
              if (l[1] !== r[1]) {
                  return l[1] < r[1] ? -1 : 1;
              }
              return 0;
          };
    var hasSortNode = function (plan) {
        assertEqual(findExecutionNodes(plan, "SortNode").length, 1, "Has SortNode");
    };
    var hasNoSortNode = function (plan) {
        assertEqual(findExecutionNodes(plan, "SortNode").length, 0, "Has NO SortNode");
    };
    var hasNoIndexRangeNode = function (plan) {
        assertEqual(findExecutionNodes(plan, "IndexRangeNode").length, 0, "Has NO IndexRangeNode");
    };
    var hasNoResultsNode = function (plan) {
        assertEqual(findExecutionNodes(plan, "NoResultsNode").length, 1, "Has NoResultsNode");
    };
    var hasCalculationNodes = function (plan, countXPect) {
        assertEqual(findExecutionNodes(plan, "CalculationNode").length,
                    countXPect,
                    "Has " + countXPect +  " CalculationNode");
    };
    var hasNoCalculationNode = function (plan) {
        assertEqual(findExecutionNodes(plan, "CalculationNode").length, 0, "Has NO CalculationNode");
    };
    var hasIndexRangeNode_WithRanges = function (plan, haveRanges) {
        var rn = findExecutionNodes(plan, "IndexRangeNode");
        assertEqual(rn.length, 1, "Has IndexRangeNode");
        if (haveRanges) {
            assertTrue(rn[0].ranges.length > 0, "Have IndexRangeNode with ranges");
        }
        else {
            assertEqual(rn[0].ranges.length, 0, "Have IndexRangeNode with NO ranges");
        }
    };
    var getRangeAttributes = function (plan) {
        var rn = findExecutionNodes(plan, "IndexRangeNode");
        assertEqual(rn.length, 1, "Has IndexRangeNode");
        assertTrue(rn[0].ranges.length > 0, "Have IndexRangeNode with ranges");
        return rn[0].ranges;
    };
    var getRangeAttribute = function (rangeAttributes, varcmp, attrcmp, getNth) {
        var ret = {};
        rangeAttributes.forEach(function compare(oneRA) {
            if ( (oneRA.variable  === varcmp) &&
                 (oneRA.attr === attrcmp)) {
                getNth --;
                if (getNth === 0) {
                    ret = oneRA;
                }
            }

        });
        return ret;
    };
    var isNodeType = function(node, type) {
        assertEqual(node.type, type, "check whether this node is of type "+type);
    };

    return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
// Datastructure: 
//  - double index on (a,b)/(f,g) for tests with these
//  - single column index on d/j to test sort behaviour without sub-columns
//  - non-indexed columns c/h to sort without indices.
//  - non-skiplist indexed columns e/j to check whether its not selecting them.
//  - join column 'joinme' to intersect both tables.
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
        var loopto;
        if (typeof loopmax === 'undefined') {
            loopto = 10;
        }
        else {
            loopto = loopmax;
        }
        /// require("internal").print("loopto: " + loopto + "\n");
 
        internal.db._drop(colName);
        skiplist = internal.db._create(colName);
        var i, j;
        for (j = 1; j <= loopto; ++j) {
            for (i = 1; i <= loopto; ++i) {
                skiplist.save({ "a" : i, "b": j , "c": j, "d": i, "e": i, "joinme" : "aoeu " + j});
            }
            skiplist.save(    { "a" : i,          "c": j, "d": i, "e": i, "joinme" : "aoeu " + j});
            skiplist.save(    {                   "c": j,                 "joinme" : "aoeu " + j});
        }

        skiplist.ensureSkiplist("a", "b");
        skiplist.ensureSkiplist("d");
        skiplist.ensureIndex({ type: "hash", fields: [ "c" ], unique: false });

        skiplist2 = internal.db._create(colNameOther);
        for (j = 1; j <= loopto; ++j) {
            for (i = 1; i <= loopto; ++i) {
                skiplist2.save({ "f" : i, "g": j , "h": j, "i": i, "j": i, "joinme" : "aoeu " + j});
            }
            skiplist2.save(    { "f" : i, "g": j,          "i": i, "j": i, "joinme" : "aoeu " + j});
            skiplist2.save(    {                   "h": j,                 "joinme" : "aoeu " + j});
        }
        skiplist2.ensureSkiplist("f", "g");
        skiplist2.ensureSkiplist("i");
        skiplist2.ensureIndex({ type: "hash", fields: [ "h" ], unique: false });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
        internal.db._drop(colName);
        internal.db._drop(colNameOther);
        skiplist = null;
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has no effect
////////////////////////////////////////////////////////////////////////////////

    testRuleNoEffect : function () {

      var queries = [ 

          "FOR v IN " + colName + " SORT v.c RETURN [v.a, v.b]",
// todo: we use an index anyways right now.          "FOR v IN " + colName + " SORT v.a DESC RETURN [v.a, v.b]",// currently only ASC supported.
          "FOR v IN " + colName + " SORT v.b, v.a  RETURN [v.a, v.b]",
          "FOR v IN " + colName + " SORT v.c RETURN [v.a, v.b]",
          "FOR v IN " + colName + " SORT v.a + 1 RETURN [v.a, v.b]",
          "FOR v IN " + colName + " SORT CONCAT(TO_STRING(v.a), \"lol\") RETURN [v.a, v.b]",
          "FOR v IN " + colName + " FILTER v.a > 2 LIMIT 3 SORT v.a RETURN [v.a, v.b]  ", // TODO: limit blocks sort atm.
          "FOR v IN " + colName + " FOR w IN " + colNameOther + " SORT v.a RETURN [v.a, v.b]"
      ];

      queries.forEach(function(query) {
          
          var result = AQL_EXPLAIN(query, { }, paramIndexFromSort);
          assertEqual([], result.plan.rules, query);
      });

    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has an effect
////////////////////////////////////////////////////////////////////////////////
    testRuleHasEffect : function () {
        var allresults;
        var queries = [ 
            
            "FOR v IN " + colName + " SORT v.d DESC RETURN [v.d]",// currently only ASC supported, but we use the index range anyways. todo: this may change.
            "FOR v IN " + colName + " SORT v.d FILTER v.a > 2 LIMIT 3 RETURN [v.d]  ",
            "FOR v IN " + colName + " FOR w IN 1..10 SORT v.d RETURN [v.d]",
            
            "FOR v IN " + colName + " LET x = (FOR w IN " + colNameOther + " RETURN w.f ) SORT v.a RETURN [v.a]"
        ];
        var QResults = [];
        var i = 0;
        queries.forEach(function(query) {
          
            var result = AQL_EXPLAIN(query, { }, paramIndexFromSort);
            assertEqual([ ruleName ], result.plan.rules, query);
            QResults[0] = AQL_EXECUTE(query, { }, paramNone).json;
            QResults[1] = AQL_EXECUTE(query, { }, paramIndexFromSort ).json;
            
            assertTrue(isEqual(QResults[0], QResults[1]), "Result " + i + " is Equal?");

            allresults = getQueryMultiplePlansAndExecutions(query, {});
            PY(allresults);
            i++;
        });

    },
/*

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has an effect, but the sort is kept in place since 
//   the index can't fullfill all the sorting.
////////////////////////////////////////////////////////////////////////////////
    testRuleHasEffectButSortsStill : function () {

        var queries = [
            "FOR v IN " + colName + " FILTER v.a == 1 SORT v.a, v.c RETURN [v.a, v.b, v.c]",
            "FOR v IN " + colName + " LET x = (FOR w IN " + colNameOther + " SORT w.j, w.h RETURN  w.f ) SORT v.a RETURN [v.a]"
        ];
        var QResults = [];
        var i = 0;
        queries.forEach(function(query) {

            var result = AQL_EXPLAIN(query, { }, paramIndexFromSort);
            assertEqual([ ruleName ], result.plan.rules);
            hasIndexRangeNode_WithRanges(result, false);
            hasSortNode(result);
            QResults[0] = AQL_EXECUTE(query, { }, paramNone).json;
            QResults[1] = AQL_EXECUTE(query, { }, paramIndexFromSort ).json;
            assertTrue(isEqual(QResults[0], QResults[1]), "Result " + i + " is Equal?");
            i++;
        });

    },



// -----------------------------------------------------------------------------
// --SECTION--                                                  sortToIndexRange
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief this sort is replaceable by an index.
////////////////////////////////////////////////////////////////////////////////
      testSortIndexable: function () {

          var query = "FOR v IN " + colName + " SORT v.a RETURN [v.a, v.b]";

          var XPresult;
          var QResults=[];
          var i;

          // we have to re-sort here, because of the index has one more sort criteria.
          QResults[0] = AQL_EXECUTE(query, { }, paramNone).json.sort(sortArray);

          // -> use-index-for-sort alone.
          XPresult    = AQL_EXPLAIN(query, { }, paramIndexFromSort);
          QResults[1] = AQL_EXECUTE(query, { }, paramIndexFromSort).json;
          // our rule should have been applied.
          assertEqual([ ruleName ], XPresult.plan.rules);
          // The sortnode and its calculation node should have been removed.
          hasNoSortNode(XPresult);
          // the dependencies of the sortnode weren't removed...
          hasCalculationNodes(XPresult, 2);
          // The IndexRangeNode created by this rule is simple; it shouldn't have ranges.
          hasIndexRangeNode_WithRanges(XPresult, false);

          // -> combined use-index-for-sort and remove-unnecessary-calculations-2
          XPresult    = AQL_EXPLAIN(query, { }, paramIndexFromSort_RemoveCalculations);
          QResults[2] = AQL_EXECUTE(query, { }, paramIndexFromSort_RemoveCalculations).json;
          // our rule should have been applied.
          assertEqual([ ruleName, removeCalculationNodes ].sort(), XPresult.plan.rules.sort());
          // The sortnode and its calculation node should have been removed.
          hasNoSortNode(XPresult);
          // now the dependencies of the sortnode should be gone:
          hasCalculationNodes(XPresult, 1);
          // The IndexRangeNode created by this rule is simple; it shouldn't have ranges.
          hasIndexRangeNode_WithRanges(XPresult, false);

          for (i = 1; i < 3; i++) {
              assertTrue(isEqual(QResults[0], QResults[i]), "Result " + i + " is Equal?");
          }

      },


////////////////////////////////////////////////////////////////////////////////
/// @brief test in detail that this rule has an effect, but the sort is kept in
//    place since the index can't fullfill all of the sorting criteria.
////////////////////////////////////////////////////////////////////////////////
      testSortMoreThanIndexed: function () {

          var query = "FOR v IN " + colName + " FILTER v.a == 1 SORT v.a, v.c RETURN [v.a, v.b, v.c]";
          // no index can be used for v.c -> sort has to remain in place!
          var XPresult;
          var QResults=[];
          var i;

          // the index we will compare to sorts by a & b, so we need to re-sort the result here to accomplish similarity.
          QResults[0] = AQL_EXECUTE(query, { }, paramNone).json.sort(sortArray);


          // -> use-index-for-sort alone.
          QResults[1] = AQL_EXECUTE(query, { }, paramIndexFromSort).json;
          XPresult    = AQL_EXPLAIN(query, { }, paramIndexFromSort);
          // our rule should be there.
          assertEqual([ ruleName ], XPresult.plan.rules);
          // The sortnode and its calculation node should have been removed.
          
          hasSortNode(XPresult);
          hasCalculationNodes(XPresult, 4);
          // The IndexRangeNode created by this rule is simple; it shouldn't have ranges.
          hasIndexRangeNode_WithRanges(XPresult, false);

          // -> combined use-index-for-sort and use-index-range
          //    use-index-range superseedes use-index-for-sort
          QResults[2] = AQL_EXECUTE(query, { }, paramIndexFromSort_IndexRange).json;
          XPresult    = AQL_EXPLAIN(query, { }, paramIndexFromSort_IndexRange);

          assertEqual([ secondRuleName ], XPresult.plan.rules.sort());
          // The sortnode and its calculation node should not have been removed.
          hasSortNode(XPresult);
          hasCalculationNodes(XPresult, 4);
          // The IndexRangeNode created by this rule should be more clever, it knows the ranges.
          hasIndexRangeNode_WithRanges(XPresult, true);

          // -> use-index-range alone.
          QResults[3] = AQL_EXECUTE(query, { }, paramIndexRange).json;
          XPresult    = AQL_EXPLAIN(query, { }, paramIndexRange);
          assertEqual([ secondRuleName ], XPresult.plan.rules);
          // the sortnode and its calculation node should be there. 

          hasSortNode(XPresult);
          hasCalculationNodes(XPresult,4);
          // we should be able to find exactly one sortnode property - its a Calculation node.
          var sortProperty = findReferencedNodes(XPresult, findExecutionNodes(XPresult, "SortNode")[0]);
          assertEqual(sortProperty.length, 2);
          assertEqual(sortProperty[0].type, "CalculationNode");
          assertEqual(sortProperty[1].type, "CalculationNode");
          // The IndexRangeNode created by this rule should be more clever, it knows the ranges.
          hasIndexRangeNode_WithRanges(XPresult, true);

          for (i = 1; i < 4; i++) {
              assertTrue(isEqual(QResults[0], QResults[i]), "Result " + i + " is Equal?");
          }
      },

////////////////////////////////////////////////////////////////////////////////
/// @brief test in detail that an index range fullfills everything the sort does, 
//   and thus the sort is removed.
////////////////////////////////////////////////////////////////////////////////
      testRangeSuperseedsSort: function () {

          var query = "FOR v IN " + colName + " FILTER v.a == 1 SORT v.a RETURN [v.a, v.b, v.c]";

          var XPresult;
          var QResults=[];
          var i;

          // the index we will compare to sorts by a & b, so we need to re-sort the result here to accomplish similarity.
          QResults[0] = AQL_EXECUTE(query, { }, paramNone).json.sort(sortArray);

          // -> use-index-for-sort alone.
          QResults[1] = AQL_EXECUTE(query, { }, paramIndexFromSort).json;
          XPresult    = AQL_EXPLAIN(query, { }, paramIndexFromSort);
          // our rule should be there.
          assertEqual([ ruleName ], XPresult.plan.rules);
          // The sortnode should be gone, its calculation node should not have been removed yet.
          hasNoSortNode(XPresult);
          hasCalculationNodes(XPresult, 3);


          // The IndexRangeNode created by this rule is simple; it shouldn't have ranges.
          hasIndexRangeNode_WithRanges(XPresult, false);

          // -> combined use-index-for-sort and use-index-range
          QResults[2] = AQL_EXECUTE(query, { }, paramIndexFromSort_IndexRange).json;
          XPresult    = AQL_EXPLAIN(query, { }, paramIndexFromSort_IndexRange);
          assertEqual([ secondRuleName, ruleName ].sort(), XPresult.plan.rules.sort());
          // The sortnode should be gone, its calculation node should not have been removed yet.
          hasNoSortNode(XPresult);
          hasCalculationNodes(XPresult, 3);
          // The IndexRangeNode created by this rule should be more clever, it knows the ranges.
          hasIndexRangeNode_WithRanges(XPresult, true);

          // -> use-index-range alone.
          QResults[3] = AQL_EXECUTE(query, { }, paramIndexRange).json;
          XPresult    = AQL_EXPLAIN(query, { }, paramIndexRange);
          assertEqual([ secondRuleName ], XPresult.plan.rules);
          // the sortnode and its calculation node should be there.
          hasSortNode(XPresult);
          hasCalculationNodes(XPresult, 3);
          // we should be able to find exactly one sortnode property - its a Calculation node.
          var sortProperty = findReferencedNodes(XPresult, findExecutionNodes(XPresult, "SortNode")[0]);
          assertEqual(sortProperty.length, 1);
          isNodeType(sortProperty[0], "CalculationNode");
          // The IndexRangeNode created by this rule should be more clever, it knows the ranges.
          hasIndexRangeNode_WithRanges(XPresult, true);

          // -> combined use-index-for-sort, remove-unnecessary-calculations-2 and use-index-range
          QResults[4] = AQL_EXECUTE(query, { }, paramIndexFromSort_IndexRange_RemoveCalculations).json;

          XPresult    = AQL_EXPLAIN(query, { }, paramIndexFromSort_IndexRange_RemoveCalculations);
          assertEqual([ secondRuleName, removeCalculationNodes, ruleName ].sort(), XPresult.plan.rules.sort());
          // the sortnode and its calculation node should be gone.
          hasNoSortNode(XPresult);
          hasCalculationNodes(XPresult, 2);
          // The IndexRangeNode created by this rule should be more clever, it knows the ranges.
          hasIndexRangeNode_WithRanges(XPresult, true);

          for (i = 1; i < 5; i++) {
              assertTrue(isEqual(QResults[0], QResults[i]), "Result " + i + " is Equal?");
          }

      },

////////////////////////////////////////////////////////////////////////////////
/// @brief test in detail that an index range fullfills everything the sort does, 
//   and thus the sort is removed; multi-dimensional indexes are utilized.
////////////////////////////////////////////////////////////////////////////////
      testRangeSuperseedsSort2: function () {

          var query = "FOR v IN " + colName + " FILTER v.a == 1 SORT v.a, v.b RETURN [v.a, v.b, v.c]";
          var XPresult;
          var QResults=[];
          var i;

          // the index we will compare to sorts by a & b, so we need to re-sort the result here to accomplish similarity.

          QResults[0] = AQL_EXECUTE(query, { }, paramNone).json;

          // -> use-index-for-sort alone.
          QResults[1] = AQL_EXECUTE(query, { }, paramIndexFromSort).json;
          XPresult    = AQL_EXPLAIN(query, { }, paramIndexFromSort);
          // our rule should be there.
          assertEqual([ ruleName ], XPresult.plan.rules);
          // The sortnode should be gone, its calculation node should not have been removed yet.
          hasNoSortNode(XPresult);
          hasCalculationNodes(XPresult, 4);
          // The IndexRangeNode created by this rule is simple; it shouldn't have ranges.
          hasIndexRangeNode_WithRanges(XPresult, false);

          // -> combined use-index-for-sort and use-index-range
          QResults[2] = AQL_EXECUTE(query, { }, paramIndexFromSort_IndexRange).json;

          XPresult    = AQL_EXPLAIN(query, { }, paramIndexFromSort_IndexRange);
          
          assertEqual([ secondRuleName, ruleName ].sort(), XPresult.plan.rules.sort());
          // The sortnode should be gone, its calculation node should not have been removed yet.
          hasNoSortNode(XPresult);

          hasCalculationNodes(XPresult, 4);
          // The IndexRangeNode created by this rule should be more clever, it knows the ranges.
          hasIndexRangeNode_WithRanges(XPresult, true);

          // -> use-index-range alone.
          QResults[3] = AQL_EXECUTE(query, { }, paramIndexRange).json;
          XPresult    = AQL_EXPLAIN(query, { }, paramIndexRange);
          assertEqual([ secondRuleName ], XPresult.plan.rules);
          // the sortnode and its calculation node should be there.
          hasSortNode(XPresult);
          hasCalculationNodes(XPresult, 4);
          // we should be able to find exactly one sortnode property - its a Calculation node.
          var sortProperty = findReferencedNodes(XPresult, findExecutionNodes(XPresult, "SortNode")[0]);

          assertEqual(sortProperty.length, 2);
          isNodeType(sortProperty[0], "CalculationNode");
          // The IndexRangeNode created by this rule should be more clever, it knows the ranges.
          hasIndexRangeNode_WithRanges(XPresult, true);

          // -> combined use-index-for-sort, remove-unnecessary-calculations-2 and use-index-range
          QResults[4] = AQL_EXECUTE(query, { }, paramIndexFromSort_IndexRange_RemoveCalculations).json;
          XPresult    = AQL_EXPLAIN(query, { }, paramIndexFromSort_IndexRange_RemoveCalculations);
          assertEqual([ ruleName, secondRuleName, removeCalculationNodes].sort(), XPresult.plan.rules.sort());
          // the sortnode and its calculation node should be there.
          hasNoSortNode(XPresult);
          hasCalculationNodes(XPresult, 2);

          // The IndexRangeNode created by this rule should be more clever, it knows the ranges.
          hasIndexRangeNode_WithRanges(XPresult, true);

          for (i = 1; i < 5; i++) {
              assertTrue(isEqual(QResults[0], QResults[i]), "Result " + i + " is Equal?");
          }
      },



// -----------------------------------------------------------------------------
// --SECTION--                                                      toIndexRange
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test in detail that an index range can be used for an equality filter.
////////////////////////////////////////////////////////////////////////////////
      testRangeEquals: function () {

          var query = "FOR v IN " + colName + " FILTER v.a == 1 RETURN [v.a, v.b, v.c]";

          var XPresult;
          var QResults=[];
          var i;

          // the index we will compare to sorts by a & b, so we need to re-sort the result here to accomplish similarity.
          QResults[0] = AQL_EXECUTE(query, { }, paramNone).json;

          // -> use-index-range alone.
          QResults[1] = AQL_EXECUTE(query, { }, paramIndexRange).json;
          XPresult    = AQL_EXPLAIN(query, { }, paramIndexRange);
          assertEqual([ secondRuleName ], XPresult.plan.rules);
          // the sortnode and its calculation node should be there.
          hasCalculationNodes(XPresult, 2);

          // The IndexRangeNode created by this rule should be more clever, it knows the ranges.
          var RAs = getRangeAttributes(XPresult);
          var first = getRangeAttribute(RAs, "v", "a", 1);
          
          assertEqual(first.lows.length, 0, "no non-constant low bounds");
          assertEqual(first.highs.length, 0, "no non-constant high bounds");
          assertEqual(first.lowConst.bound, 1, "correctness of bound");
          assertEqual(first.lowConst.bound, first.highConst.bound, "bounds equality");

          for (i = 1; i < 2; i++) {
              assertTrue(isEqual(QResults[0].sort(sortArray), QResults[i]), "Result " + i + " is Equal?");
          }

      },


////////////////////////////////////////////////////////////////////////////////
/// @brief test in detail that an index range can be used for a less than filter.
////////////////////////////////////////////////////////////////////////////////
      testRangeLessThan: function () {
          var query = "FOR v IN " + colName + " FILTER v.a < 5 RETURN [v.a, v.b]";

          var XPresult;
          var QResults=[];

          // the index we will compare to sorts by a & b, so we need to re-sort the result here to accomplish similarity.
          QResults[0] = AQL_EXECUTE(query, { }, paramNone).json.sort(sortArray);

          // -> use-index-range alone.
          QResults[1] = AQL_EXECUTE(query, { }, paramIndexRange).json;

          XPresult    = AQL_EXPLAIN(query, { }, paramIndexRange);
          assertEqual([ secondRuleName ], XPresult.plan.rules);
          // the sortnode and its calculation node should be there.
          hasCalculationNodes(XPresult, 2);

          // The IndexRangeNode created by this rule should be more clever, it knows the ranges.
          var RAs = getRangeAttributes(XPresult);
          var first = getRangeAttribute(RAs, "v", "a", 1);
          assertEqual(first.lowConst.bound, undefined, "no constant lower bound");
          assertEqual(first.lows.length, 0, "no variable low bound");
          assertEqual(first.highs.length, 0, "no variable high bound");
          assertEqual(first.highConst.bound, 5, "proper value was set");

          assertTrue(isEqual(QResults[0], QResults[1]), "Results are Equal?");
      },

////////////////////////////////////////////////////////////////////////////////
/// @brief test in detail that an index range can be used for a greater than filter.
////////////////////////////////////////////////////////////////////////////////
      testRangeGreaterThan: function () {
          var query = "FOR v IN " + colName + " FILTER v.a > 5 RETURN [v.a, v.b]";
          var XPresult;
          var QResults=[];

          // the index we will compare to sorts by a & b, so we need to re-sort the result here to accomplish similarity.
          QResults[0] = AQL_EXECUTE(query, { }, paramNone).json.sort(sortArray);

          // -> use-index-range alone.
          QResults[1] = AQL_EXECUTE(query, { }, paramIndexRange).json;

          XPresult    = AQL_EXPLAIN(query, { }, paramIndexRange);
          assertEqual([ secondRuleName ], XPresult.plan.rules);
          // the sortnode and its calculation node should be there.
          hasCalculationNodes(XPresult, 2);

          // The IndexRangeNode created by this rule should be more clever, it knows the ranges.
          var RAs = getRangeAttributes(XPresult);
          var first = getRangeAttribute(RAs, "v", "a", 1);
          
          assertEqual(first.highConst.bound, undefined, "no constant upper bound");
          assertEqual(first.highs.length, 0, "no variable high bound");
          assertEqual(first.lows.length, 0, "no variable low bound");
          assertEqual(first.lowConst.bound, 5, "proper value was set");

          assertTrue(isEqual(QResults[0], QResults[1]), "Results are Equal?");

      },

////////////////////////////////////////////////////////////////////////////////
/// @brief test in detail that an index range can be used for an and combined 
///   greater than + less than filter spanning a range.
////////////////////////////////////////////////////////////////////////////////
      testRangeBandpass: function () {
          var query = "FOR v IN " + colName + " FILTER v.a > 4 && v.a < 10 RETURN [v.a, v.b]";
          var XPresult;
          var QResults=[];

          // the index we will compare to sorts by a & b, so we need to re-sort the result here to accomplish similarity.
          QResults[0] = AQL_EXECUTE(query, { }, paramNone).json.sort(sortArray);

          // -> use-index-range alone.
          QResults[1] = AQL_EXECUTE(query, { }, paramIndexRange).json;

          XPresult    = AQL_EXPLAIN(query, { }, paramIndexRange);
          assertEqual([ secondRuleName ], XPresult.plan.rules);
          // the sortnode and its calculation node should be there.
          hasCalculationNodes(XPresult, 2);

          // The IndexRangeNode created by this rule should be more clever, it knows the ranges.
          var RAs = getRangeAttributes(XPresult);
          var first = getRangeAttribute(RAs, "v", "a", 1);
          
          assertEqual(first.highs.length, 0, "no variable high bounds");
          assertEqual(first.lows.length, 0, "no variable low bounds");
          assertEqual(first.lowConst.bound, 4, "proper value was set");
          assertEqual(first.highConst.bound, 10, "proper value was set");

          assertTrue(isEqual(QResults[0], QResults[1]), "Results are Equal?");
      },

////////////////////////////////////////////////////////////////////////////////
/// @brief test in detail that an index range can be used for an and combined 
///   greater than + less than filter spanning an empty range. This actually
///   recognises the empty range and introduces a NoResultsNode but not an
///   IndexRangeNode.
////////////////////////////////////////////////////////////////////////////////

      testRangeBandpassInvalid: function () {
        var query = "FOR v IN " + colName + " FILTER v.a > 7 && v.a < 4 RETURN [v.a, v.b]";

        var XPresult;
        var QResults=[];

        // the index we will compare to sorts by a & b, so we need to re-sort the result here to accomplish similarity.
        QResults[0] = AQL_EXECUTE(query, { }, paramNone).json.sort(sortArray);

        // -> use-index-range alone.
        QResults[1] = AQL_EXECUTE(query, { }, paramIndexRange).json;


        XPresult    = AQL_EXPLAIN(query, { }, paramIndexRange);
        assertEqual([ secondRuleName ], XPresult.plan.rules);
        // the sortnode and its calculation node should be there.
        hasCalculationNodes(XPresult, 2);

        hasNoResultsNode(XPresult);
        hasNoIndexRangeNode(XPresult);

        assertTrue(isEqual(QResults[0], QResults[1]), "Results are Equal?");
          
      },


////////////////////////////////////////////////////////////////////////////////
/// @brief test in detail that an index range can be used for an or combined 
///   greater than + less than filter spanning a range. TODO: doesn't work now.
////////////////////////////////////////////////////////////////////////////////
      testRangeBandstop: function () {
// TODO: OR  isn't implemented
//        var query = "FOR v IN " + colName + " FILTER v.a < 5 || v.a > 10 RETURN [v.a, v.b]";
//
//        var XPresult;
//        var QResults=[];
//
//        // the index we will compare to sorts by a & b, so we need to re-sort the result here to accomplish similarity.
//        QResults[0] = AQL_EXECUTE(query, { }, paramNone).json.sort(sortArray);
//
//        // -> use-index-range alone.
//        QResults[1] = AQL_EXECUTE(query, { }, paramIndexRange).json;
//
//        XPresult    = AQL_EXPLAIN(query, { }, paramIndexRange);
//        assertEqual([ secondRuleName ], XPresult.plan.rules);
//        // the sortnode and its calculation node should be there.
//        hasCalculationNodes(XPresult, 2);
//
//        // The IndexRangeNode created by this rule should be more clever, it knows the ranges.
//        var RAs = getRangeAttributes(XPresult);
//        require("internal").print(RAs);
//        var first = getRangeAttribute(RAs, "v", "a", 1);
//        
//        require("internal").print(first);
//        assertEqual(first.low.bound.vType, "int", "Type is int");
//        assertEqual(first.low.bound.value, 5, "proper value was set");
//
//        require("internal").print(QResults[0]);
//        require("internal").print(QResults[1]);              
//        assertTrue(isEqual(QResults[0], QResults[1]), "Results are Equal?");
      },

////////////////////////////////////////////////////////////////////////////////
/// @brief test in detail that an index range can be used for an or combined 
///   greater than + less than filter spanning multiple ranges. TODO: doesn't work now.
////////////////////////////////////////////////////////////////////////////////
      testMultiRangeBandpass: function () {
// TODO: OR  isn't implemented
//          var query = "FOR v IN " + colName + " FILTER ((v.a > 3 && v.a < 5) || (v.a > 4 && v.a < 7)) RETURN [v.a, v.b]";
//
//          var XPresult;
//          var QResults=[];
//
//          // the index we will compare to sorts by a & b, so we need to re-sort the result here to accomplish similarity.
//          QResults[0] = AQL_EXECUTE(query, { }, paramNone).json.sort(sortArray);
//
//          // -> use-index-range alone.
//          QResults[1] = AQL_EXECUTE(query, { }, paramIndexRange).json;
//
//          XPresult    = AQL_EXPLAIN(query, { }, paramIndexRange);
//          assertEqual([ secondRuleName ], XPresult.plan.rules);
//          // the sortnode and its calculation node should be there.
//          hasCalculationNodes(XPresult, 2);
//
//          // The IndexRangeNode created by this rule should be more clever, it knows the ranges.
//          var RAs = getRangeAttributes(XPresult);
////          require("internal").print(RAs);
//          var first = getRangeAttribute(RAs, "v", "a", 1);
//          
////          require("internal").print(first);
//          assertEqual(first.low.bound.vType, "int", "Type is int");
//          assertEqual(first.low.bound.value, 4, "proper value was set");
//          assertEqual(first.high.bound.vType, "int", "Type is int");
//          assertEqual(first.high.bound.value, 10, "proper value was set");
//
//          assertTrue(isEqual(QResults[0], QResults[1]), "Results are Equal?");
          
      }
*/
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
