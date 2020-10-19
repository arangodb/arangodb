/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global AQL_EXPLAIN, AQL_EXECUTE */

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

const internal = require("internal");
const db = internal.db;
const jsunity = require("jsunity");
const assert = require("jsunity").jsUnity.assertions;
const {assertEqual, assertFalse, assertTrue, assertNotEqual} = assert;
const helper = require("@arangodb/aql-helper");
const isEqual = helper.isEqual;
const findExecutionNodes = helper.findExecutionNodes;
const findReferencedNodes = helper.findReferencedNodes;
const getQueryMultiplePlansAndExecutions = helper.getQueryMultiplePlansAndExecutions;
const removeAlwaysOnClusterRules = helper.removeAlwaysOnClusterRules;
  
const ruleName = "use-index-for-sort";

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerRuleTestSuite() {
  var secondRuleName = "use-indexes";
  var removeCalculationNodes = "remove-unnecessary-calculations-2";
  var colName = "UnitTestsAqlOptimizer" + ruleName.replace(/-/g, "_");
  var colNameOther = colName + "_XX";

  // various choices to control the optimizer: 
  var paramNone = { optimizer: { rules: [ "-all" ] } };
  var paramIndexFromSort  = { optimizer: { rules: [ "-all", "+" + ruleName ] } };
  var paramIndexRange   = { optimizer: { rules: [ "-all", "+" + secondRuleName ] } };
  var paramIndexFromSort_IndexRange = { optimizer: { rules: [ "-all", "+" + ruleName, "+" + secondRuleName ] } };
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
    assertEqual(findExecutionNodes(plan, "SortNode").length, 0, "Has no SortNode");
  };
  var hasNoIndexNode = function (plan) {
    assertEqual(findExecutionNodes(plan, "IndexNode").length, 0, "Has no IndexNode");
  };
  var hasNoResultsNode = function (plan) {
    assertEqual(findExecutionNodes(plan, "NoResultsNode").length, 1, "Has NoResultsNode");
  };
  var hasCalculationNodes = function (plan, countXPect) {
    assertEqual(findExecutionNodes(plan, "CalculationNode").length,
                countXPect, "Has " + countXPect +  " CalculationNode");
  };
  var hasIndexNode = function (plan) {
    var rn = findExecutionNodes(plan, "IndexNode");
    assertEqual(rn.length, 1, "Has IndexNode");
    return;
  };
  var isNodeType = function(node, type) {
    assertEqual(node.type, type, "check whether this node is of type "+type);
  };

  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    // Datastructure: 
    //  - double index on (a,b)/(f,g) for tests with these
    //  - single column index on d/j to test sort behavior without sub-columns
    //  - non-indexed columns c/h to sort without indices.
    //  - non-skiplist indexed columns e/j to check whether its not selecting them.
    //  - join column 'joinme' to intersect both tables.
    ////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      var loopto = 10;

      internal.db._drop(colName);
      skiplist = internal.db._create(colName, {numberOfShards: 1});
      var i, j;
      let docs = [];
      for (j = 1; j <= loopto; ++j) {
        for (i = 1; i <= loopto; ++i) {
          docs.push({ "a" : i, "b": j , "c": j, "d": i, "e": i, "joinme" : "aoeu " + j});
        }
        docs.push(  { "a" : i,          "c": j, "d": i, "e": i, "joinme" : "aoeu " + j});
        docs.push(  {                   "c": j,                 "joinme" : "aoeu " + j});
      }
      skiplist.insert(docs);

      skiplist.ensureSkiplist("a", "b");
      skiplist.ensureSkiplist("d");
      skiplist.ensureIndex({ type: "hash", fields: [ "c" ], unique: false });

      internal.db._drop(colNameOther);
      skiplist2 = internal.db._create(colNameOther, {numberOfShards: 1});
      docs = [];
      for (j = 1; j <= loopto; ++j) {
        for (i = 1; i <= loopto; ++i) {
          docs.push({ "f" : i, "g": j , "h": j, "i": i, "j": i, "joinme" : "aoeu " + j});
        }
        docs.push(  { "f" : i, "g": j,          "i": i, "j": i, "joinme" : "aoeu " + j});
        docs.push(  {                   "h": j,                 "joinme" : "aoeu " + j});
      }
      skiplist2.insert(docs);
 
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

    testRuleOptimizeWhenEqComparison : function () {
      // skiplist: a, b
      // skiplist: d
      // hash: c
      // hash: y,z
      skiplist.ensureIndex({ type: "hash", fields: [ "y", "z" ], unique: false });
      
      var queries = [ 
        [ "FOR v IN " + colName + " FILTER v.u == 1 SORT v.u RETURN 1", false, true ],
        [ "FOR v IN " + colName + " FILTER v.c == 1 SORT v.c RETURN 1", true, false ],
        [ "FOR v IN " + colName + " FILTER v.c == 1 SORT v.z RETURN 1", false, true ],
        [ "FOR v IN " + colName + " FILTER v.c == 1 SORT v.f RETURN 1", false, true ],
        [ "FOR v IN " + colName + " FILTER v.y == 1 SORT v.z RETURN 1", true, false ],
        [ "FOR v IN " + colName + " FILTER v.y == 1 SORT v.y RETURN 1", true, false ],
        [ "FOR v IN " + colName + " FILTER v.z == 1 SORT v.y RETURN 1", true, false ],
        [ "FOR v IN " + colName + " FILTER v.z == 1 SORT v.z RETURN 1", false, true ],
        [ "FOR v IN " + colName + " FILTER v.y == 1 && v.z == 1 SORT v.y RETURN 1", true, false ],
        [ "FOR v IN " + colName + " FILTER v.y == 1 && v.z == 1 SORT v.z RETURN 1", true, false ],
        [ "FOR v IN " + colName + " FILTER v.y == 1 && v.z == 1 SORT v.y, v.z RETURN 1", true, false ],
        [ "FOR v IN " + colName + " FILTER v.y == 1 && v.z == 1 SORT v.z, v.y RETURN 1", true, false ], 
        [ "FOR v IN " + colName + " FILTER v.d == 1 SORT v.d RETURN 1", true, false ],
        [ "FOR v IN " + colName + " FILTER v.d == 1 && v.e == 1 SORT v.d RETURN 1", true, false ],
        [ "FOR v IN " + colName + " FILTER v.d == 1 SORT v.e RETURN 1", false, true ],
        [ "FOR v IN " + colName + " FILTER v.a == 1 SORT v.a, v.b RETURN 1", true, false ],
        [ "FOR v IN " + colName + " FILTER v.a == 1 && v.b == 1 SORT v.a, v.b RETURN 1", true, false ],
        [ "FOR v IN " + colName + " FILTER v.a == 1 SORT v.a RETURN 1", true, false ],
        [ "FOR v IN " + colName + " FILTER v.a == 1 SORT v.a, v.b RETURN 1", true, false ],
        [ "FOR v IN " + colName + " FILTER v.a == 1 SORT v.b RETURN 1", true, false ],
        [ "FOR v IN " + colName + " FILTER v.a == 1 && v.b == 1 SORT v.b RETURN 1", true, false ],
        [ "FOR v IN " + colName + " FILTER v.b == 1 SORT v.a, v.b RETURN 1", true, false ],
        [ "FOR v IN " + colName + " FILTER v.b == 1 SORT v.b RETURN 1", false, true ],
        [ "FOR v IN " + colName + " FILTER v.b == 1 SORT v.b, v.a RETURN 1", false, true ],
        [ "FOR v IN " + colName + " FILTER v.a == 1 && v.b == 1 SORT v.b, v.a RETURN 1", true, false ],
        [ "FOR v IN " + colName + " FILTER v.a == 1 && v.b == 1 SORT v.a, v.b RETURN 1", true, false ],
        [ "FOR v IN " + colName + " FILTER v.a == 1 && v.b == 1 SORT v.a, v.c RETURN 1", false, true ],
        [ "FOR v IN " + colName + " FILTER v.a == 1 && v.b == 1 SORT v.b, v.a RETURN 1", true, false ],
        [ "FOR v IN " + colName + " FILTER v.a == 1 && v.b == 1 SORT v.a, v.b, v.c RETURN 1", false, true ]
      ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query[0]);
        if (query[1]) {
          assertNotEqual(-1, removeAlwaysOnClusterRules(result.plan.rules).indexOf(ruleName), query[0]);
        } else {
          assertEqual(-1, removeAlwaysOnClusterRules(result.plan.rules).indexOf(ruleName), query[0]);
        }
        if (query[2]) {
          hasSortNode(result);
        } else {
          hasNoSortNode(result);
        }
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test that rule has no effect
    ////////////////////////////////////////////////////////////////////////////////

    testRuleNoEffect : function () {
      var j;
      var queries = [ 

        ["FOR v IN " + colName + " SORT v.c RETURN [v.a, v.b]", true, true],
        ["FOR v IN " + colName + " SORT v.b, v.a  RETURN [v.a]", true],
        ["FOR v IN " + colName + " SORT v.c RETURN [v.a, v.b]", true, true],
        ["FOR v IN " + colName + " SORT v.a + 1 RETURN [v.a]", false],
        ["FOR v IN " + colName + " SORT CONCAT(TO_STRING(v.a), \"lol\") RETURN [v.a]", true],
        // TODO: limit blocks sort atm.
        ["FOR v IN " + colName + " FILTER v.a > 2 LIMIT 3 SORT v.a RETURN [v.a]", true],
        ["FOR v IN " + colName + " FOR w IN " + colNameOther + " SORT v.a RETURN [v.a]", true],
        ["FOR v IN " + colName + " FOR w IN 1..10 SORT v.d RETURN [v.d]", false],
      ];

      queries.forEach(function(query) {
        
        var result = AQL_EXPLAIN(query[0], { }, paramIndexFromSort);
          if (query.length === 3 && query[2]) {
            assertEqual(["use-index-for-sort"], removeAlwaysOnClusterRules(result.plan.rules), query);
          } else {
            assertEqual([], removeAlwaysOnClusterRules(result.plan.rules), query);
          }
        if (!query[1]) {
          var allresults = getQueryMultiplePlansAndExecutions(query[0], {});
          for (j = 1; j < allresults.results.length; j++) {
            assertTrue(isEqual(allresults.results[0],
                               allresults.results[j]),
                       "while execution of '" + query[0] +
                       "' this plan gave the wrong results: " + JSON.stringify(allresults.plans[j]) +
                       " Should be: '" + JSON.stringify(allresults.results[0]) +
                       "' but Is: " + JSON.stringify(allresults.results[j]) + "'"
                      );
          }
        }
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test that rule has an effect
    ////////////////////////////////////////////////////////////////////////////////

    testRuleHasEffect : function () {
      var allresults;
      var queries = [ 
        "FOR v IN " + colName + " SORT v.d DESC RETURN [v.d]",
        "FOR v IN " + colName + " SORT v.d ASC RETURN [v.d]",

        "FOR v IN " + colName + " SORT v.d FILTER v.a > 2 LIMIT 3 RETURN [v.d]  ",

        "FOR v IN " + colName + " LET x = (FOR w IN " + colNameOther + " RETURN w.f ) SORT v.a RETURN [v.a]"
      ];
      var QResults = [];
      var i = 0;
      queries.forEach(function(query) {
        var j;

        var result = AQL_EXPLAIN(query, { }, paramIndexFromSort);
        assertEqual([ ruleName ], 
                    removeAlwaysOnClusterRules(result.plan.rules), query);
        QResults[0] = AQL_EXECUTE(query, { }, paramNone).json;
        QResults[1] = AQL_EXECUTE(query, { }, paramIndexFromSort).json;
      
        assertTrue(isEqual(QResults[0], QResults[1]), "result " + i + " is equal?");

        allresults = getQueryMultiplePlansAndExecutions(query, {});
        for (j = 1; j < allresults.results.length; j++) {
            assertTrue(isEqual(allresults.results[0],
                               allresults.results[j]),
                       "while execution of '" + query +
                       "' this plan gave the wrong results: " + JSON.stringify(allresults.plans[j]) +
                       " Should be: '" + JSON.stringify(allresults.results[0]) +
                       "' but Is: " + JSON.stringify(allresults.results[j]) + "'"
                      );
        }
        i++;
      });

    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test that rule has an effect, but the sort is kept in place since 
    //   the index can't fullfill all the sorting.
    ////////////////////////////////////////////////////////////////////////////////
    testRuleHasEffectButSortsStill : function () {
      var queries = [
        "FOR v IN " + colName + " FILTER v.a == 1 SORT v.a, v.c RETURN [v.a, v.b, v.c]",
        "FOR v IN " + colName + " LET x = (FOR w IN " 
          + colNameOther + " SORT w.j, w.h RETURN  w.f ) SORT v.a RETURN [v.a]"
      ];
      var QResults = [];
      var i = 0;
      queries.forEach(function(query) {
        var j;
        var result = AQL_EXPLAIN(query, { }, paramIndexFromSort);
        assertEqual([ ruleName ], removeAlwaysOnClusterRules(result.plan.rules));
        hasIndexNode(result);
        hasSortNode(result);
        QResults[0] = AQL_EXECUTE(query, { }, paramNone).json;
        QResults[1] = AQL_EXECUTE(query, { }, paramIndexFromSort ).json;
        assertTrue(isEqual(QResults[0], QResults[1]), "Result " + i + " is Equal?");

        var allresults = getQueryMultiplePlansAndExecutions(query, {});
        for (j = 1; j < allresults.results.length; j++) {
          assertTrue(isEqual(allresults.results[0],
                             allresults.results[j]),
                     "while execution of '" + query +
                     "' this plan gave the wrong results: " + JSON.stringify(allresults.plans[j]) +
                     " Should be: '" + JSON.stringify(allresults.results[0]) +
                     "' but Is: " + JSON.stringify(allresults.results[j]) + "'"
                    );
        }
        i++;
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief this sort is replaceable by an index.
    ////////////////////////////////////////////////////////////////////////////////

    testSortIndexable: function () {
      var query = "FOR v IN " + colName + " SORT v.a RETURN [v.a, v.b]";

      var XPresult;
      var QResults=[];
      var i, j;

      // we have to re-sort here, because of the index has one more sort criteria.
      QResults[0] = AQL_EXECUTE(query, { }, paramNone).json.sort(sortArray);

      // -> use-index-for-sort alone.
      XPresult    = AQL_EXPLAIN(query, { }, paramIndexFromSort);
      QResults[1] = AQL_EXECUTE(query, { }, paramIndexFromSort).json.sort(sortArray);
      // our rule should have been applied.
      assertEqual([ ruleName ], removeAlwaysOnClusterRules(XPresult.plan.rules));
      // The sortnode and its calculation node should have been removed.
      hasNoSortNode(XPresult);
      // the dependencies of the sortnode weren't removed...
      hasCalculationNodes(XPresult, 2);
      // The IndexNode created by this rule is simple; it shouldn't have ranges.
      hasIndexNode(XPresult);

      // -> combined use-index-for-sort and remove-unnecessary-calculations-2
      XPresult    = AQL_EXPLAIN(query, { }, paramIndexFromSort_RemoveCalculations);
      QResults[2] = AQL_EXECUTE(query, { }, paramIndexFromSort_RemoveCalculations).json.sort(sortArray);
      // our rule should have been applied.
      assertEqual([ ruleName, removeCalculationNodes ].sort(), removeAlwaysOnClusterRules(XPresult.plan.rules).sort());
      // The sortnode and its calculation node should have been removed.
      hasNoSortNode(XPresult);
      // now the dependencies of the sortnode should be gone:
      hasCalculationNodes(XPresult, 1);
      // The IndexNode created by this rule is simple; it shouldn't have ranges.
      hasIndexNode(XPresult);

      for (i = 1; i < 3; i++) {
        assertTrue(isEqual(QResults[0], QResults[i]), "Result " + i + " is equal?");
      }
      var allresults = getQueryMultiplePlansAndExecutions(query, {});
      for (j = 1; j < allresults.results.length; j++) {
        if (findExecutionNodes(allresults.plans[j], "IndexNode").length === 0) {
          // This plan didn't sort by the index, so we need to re-sort the result by v.a and v.b
          assertTrue(isEqual(allresults.results[0].json.sort(sortArray),
                             allresults.results[j].json.sort(sortArray)),
                     "while execution of '" + query +
                     "' this plan gave the wrong results: " + JSON.stringify(allresults.plans[j]) +
                     " Should be: '" + JSON.stringify(allresults.results[0]) +
                     "' but Is: " + JSON.stringify(allresults.results[j]) + "'"
                    );

        }
        else {
          assertTrue(isEqual(allresults.results[0].json.sort(sortArray),
                             allresults.results[j].json),
                     "while execution of '" + query +
                     "' this plan gave the wrong results: " + JSON.stringify(allresults.plans[j]) +
                     " Should be: '" + JSON.stringify(allresults.results[0]) +
                     "' but Is: " + JSON.stringify(allresults.results[j]) + "'"
                    );
        }
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
      var i, j;

      // the index we will compare to sorts by a & b, so we need to
      // re-sort the result here to accomplish similarity.
      QResults[0] = AQL_EXECUTE(query, { }, paramNone).json.sort(sortArray);


      // -> use-index-for-sort alone.
      QResults[1] = AQL_EXECUTE(query, { }, paramIndexFromSort).json;
      XPresult    = AQL_EXPLAIN(query, { }, paramIndexFromSort);
      // our rule should be there.
      assertEqual([ ruleName ], removeAlwaysOnClusterRules(XPresult.plan.rules));
      // The sortnode and its calculation node should have been removed.
      
      hasSortNode(XPresult);
      hasCalculationNodes(XPresult, 4);
      // The IndexNode created by this rule is simple; it shouldn't have ranges.
      hasIndexNode(XPresult);

      // -> combined use-index-for-sort and use-index-range
      //    use-index-range supersedes use-index-for-sort
      QResults[2] = AQL_EXECUTE(query, { }, paramIndexFromSort_IndexRange).json;
      XPresult    = AQL_EXPLAIN(query, { }, paramIndexFromSort_IndexRange);

      assertEqual([ secondRuleName ], removeAlwaysOnClusterRules(XPresult.plan.rules).sort());
      // The sortnode and its calculation node should not have been removed.
      hasSortNode(XPresult);
      hasCalculationNodes(XPresult, 4);
      // The IndexNode created by this rule should be more clever, it knows the ranges.
      hasIndexNode(XPresult);

      // -> use-index-range alone.
      QResults[3] = AQL_EXECUTE(query, { }, paramIndexRange).json;
      XPresult    = AQL_EXPLAIN(query, { }, paramIndexRange);
      assertEqual([ secondRuleName ], removeAlwaysOnClusterRules(XPresult.plan.rules));
      // the sortnode and its calculation node should be there. 

      hasSortNode(XPresult);
      hasCalculationNodes(XPresult,4);
      // we should be able to find exactly one sortnode property - its a Calculation node.
      var sortProperty = findReferencedNodes(XPresult, findExecutionNodes(XPresult, "SortNode")[0]);
      assertEqual(sortProperty.length, 2);
      assertEqual(sortProperty[0].type, "CalculationNode");
      assertEqual(sortProperty[1].type, "CalculationNode");
      // The IndexNode created by this rule should be more clever, it knows the ranges.
      hasIndexNode(XPresult);

      for (i = 1; i < 4; i++) {
        assertTrue(isEqual(QResults[0], QResults[i]), "Result " + i + " is Equal?");
      }
      var allresults = getQueryMultiplePlansAndExecutions(query, {});
      for (j = 1; j < allresults.results.length; j++) {
        assertTrue(isEqual(allresults.results[0],
                           allresults.results[j]),
                   "while execution of '" + query +
                   "' this plan gave the wrong results: " + JSON.stringify(allresults.plans[j]) +
                   " Should be: '" + JSON.stringify(allresults.results[0]) +
                   "' but Is: " + JSON.stringify(allresults.results[j]) + "'"
                  );
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test in detail that an index range fullfills everything the sort does, 
    //   and thus the sort is removed.
    ////////////////////////////////////////////////////////////////////////////////

    testRangeSupersedesSort: function () {

      var query = "FOR v IN " + colName + " FILTER v.a == 1 SORT v.a RETURN [v.a, v.b, v.c]";

      var XPresult;
      var QResults=[];
      var i, j;

      // the index we will compare to sorts by a & b, so we need to
      // re-sort the result here to accomplish similarity.
      QResults[0] = AQL_EXECUTE(query, { }, paramNone).json.sort(sortArray);

      // -> use-index-for-sort alone.
      QResults[1] = AQL_EXECUTE(query, { }, paramIndexFromSort).json.sort(sortArray);
      XPresult    = AQL_EXPLAIN(query, { }, paramIndexFromSort);
      // our rule should be there.
      assertEqual([ ruleName ], removeAlwaysOnClusterRules(XPresult.plan.rules));
      // The sortnode should be gone, its calculation node should not have been removed yet.
      hasNoSortNode(XPresult);
      hasCalculationNodes(XPresult, 3);


      // The IndexNode created by this rule is simple; it shouldn't have ranges.
      hasIndexNode(XPresult);

      // -> combined use-index-for-sort and use-index-range
      QResults[2] = AQL_EXECUTE(query, { }, paramIndexFromSort_IndexRange).json.sort(sortArray);
      XPresult    = AQL_EXPLAIN(query, { }, paramIndexFromSort_IndexRange);
      assertEqual([ secondRuleName, ruleName ].sort(), removeAlwaysOnClusterRules(XPresult.plan.rules).sort());
      // The sortnode should be gone, its calculation node should not have been removed yet.
      hasNoSortNode(XPresult);
      hasCalculationNodes(XPresult, 3);
      // The IndexNode created by this rule should be more clever, it knows the ranges.
      hasIndexNode(XPresult);

      // -> use-index-range alone.
      QResults[3] = AQL_EXECUTE(query, { }, paramIndexRange).json.sort(sortArray);
      XPresult    = AQL_EXPLAIN(query, { }, paramIndexRange);
      assertEqual([ secondRuleName ], removeAlwaysOnClusterRules(XPresult.plan.rules));
      // the sortnode and its calculation node should be there.
      hasSortNode(XPresult);
      hasCalculationNodes(XPresult, 3);
      // we should be able to find exactly one sortnode property - its a Calculation node.
      var sortProperty = findReferencedNodes(XPresult, findExecutionNodes(XPresult, "SortNode")[0]);
      assertEqual(sortProperty.length, 1);
      isNodeType(sortProperty[0], "CalculationNode");
      // The IndexNode created by this rule should be more clever, it knows the ranges.
      hasIndexNode(XPresult);

      // -> combined use-index-for-sort, remove-unnecessary-calculations-2 and use-index-range
      QResults[4] = AQL_EXECUTE(query, { }, paramIndexFromSort_IndexRange_RemoveCalculations).json.sort(sortArray);

      XPresult    = AQL_EXPLAIN(query, { }, paramIndexFromSort_IndexRange_RemoveCalculations);
      assertEqual([ secondRuleName, removeCalculationNodes, ruleName ].sort(), removeAlwaysOnClusterRules(XPresult.plan.rules).sort());
      // the sortnode and its calculation node should be gone.
      hasNoSortNode(XPresult);
      hasCalculationNodes(XPresult, 2);
      // The IndexNode created by this rule should be more clever, it knows the ranges.
      hasIndexNode(XPresult);

      for (i = 1; i < 5; i++) {
        assertTrue(isEqual(QResults[0], QResults[i]), "Result " + i + " is equal?");
      }
      var allresults = getQueryMultiplePlansAndExecutions(query, {});
      for (j = 1; j < allresults.results.length; j++) {
        assertTrue(isEqual(allresults.results[0].json.sort(sortArray),
                           allresults.results[j].json.sort(sortArray)),
                   "while execution of '" + query +
                   "' this plan gave the wrong results: " + JSON.stringify(allresults.plans[j]) +
                   " Should be: '" + JSON.stringify(allresults.results[0]) +
                   "' but Is: " + JSON.stringify(allresults.results[j]) + "'"
                  );
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test in detail that an index range fullfills everything the sort does, 
    //   and thus the sort is removed; multi-dimensional indexes are utilized.
    ////////////////////////////////////////////////////////////////////////////////

    testRangeSupersedesSort2: function () {
      var query = "FOR v IN " + colName + " FILTER v.a == 1 SORT v.a, v.b RETURN [v.a, v.b, v.c]";
      var XPresult;
      var QResults=[];
      var i, j;

      // the index we will compare to sorts by a & b, so we need to
      // re-sort the result here to accomplish similarity.

      QResults[0] = AQL_EXECUTE(query, { }, paramNone).json;

      // -> use-index-for-sort alone.
      QResults[1] = AQL_EXECUTE(query, { }, paramIndexFromSort).json;
      XPresult    = AQL_EXPLAIN(query, { }, paramIndexFromSort);
      // our rule should be there.
      assertEqual([ ruleName ], removeAlwaysOnClusterRules(XPresult.plan.rules));
      // The sortnode should be gone, its calculation node should not have been removed yet.
      hasNoSortNode(XPresult);
      hasCalculationNodes(XPresult, 4);
      // The IndexNode created by this rule is simple; it shouldn't have ranges.
      hasIndexNode(XPresult);

      // -> combined use-index-for-sort and use-index-range
      QResults[2] = AQL_EXECUTE(query, { }, paramIndexFromSort_IndexRange).json;

      XPresult    = AQL_EXPLAIN(query, { }, paramIndexFromSort_IndexRange);
      
      assertEqual([ secondRuleName, ruleName ].sort(), removeAlwaysOnClusterRules(XPresult.plan.rules).sort());
      // The sortnode should be gone, its calculation node should not have been removed yet.
      hasNoSortNode(XPresult);

      hasCalculationNodes(XPresult, 4);
      // The IndexNode created by this rule should be more clever, it knows the ranges.
      hasIndexNode(XPresult);

      // -> use-index-range alone.
      QResults[3] = AQL_EXECUTE(query, { }, paramIndexRange).json;
      XPresult    = AQL_EXPLAIN(query, { }, paramIndexRange);
      assertEqual([ secondRuleName ], removeAlwaysOnClusterRules(XPresult.plan.rules));
      // the sortnode and its calculation node should be there.
      hasSortNode(XPresult);
      hasCalculationNodes(XPresult, 4);
      // we should be able to find exactly one sortnode property - its a Calculation node.
      var sortProperty = findReferencedNodes(XPresult, findExecutionNodes(XPresult, "SortNode")[0]);

      assertEqual(sortProperty.length, 2);
      isNodeType(sortProperty[0], "CalculationNode");
      // The IndexNode created by this rule should be more clever, it knows the ranges.
      hasIndexNode(XPresult);

      // -> combined use-index-for-sort, remove-unnecessary-calculations-2 and use-index-range
      QResults[4] = AQL_EXECUTE(query, { }, paramIndexFromSort_IndexRange_RemoveCalculations).json;
      XPresult    = AQL_EXPLAIN(query, { }, paramIndexFromSort_IndexRange_RemoveCalculations);
      assertEqual([ ruleName, secondRuleName, removeCalculationNodes].sort(), removeAlwaysOnClusterRules(XPresult.plan.rules).sort());
      // the sortnode and its calculation node should be there.
      hasNoSortNode(XPresult);
      hasCalculationNodes(XPresult, 2);

      // The IndexNode created by this rule should be more clever, it knows the ranges.
      hasIndexNode(XPresult);

      for (i = 1; i < 5; i++) {
        assertTrue(isEqual(QResults[0], QResults[i]), "Result " + i + " is Equal?");
      }
      var allresults = getQueryMultiplePlansAndExecutions(query, {});
      for (j = 1; j < allresults.results.length; j++) {
        assertTrue(isEqual(allresults.results[0],
                           allresults.results[j]),
                   "while execution of '" + query +
                   "' this plan gave the wrong results: " + JSON.stringify(allresults.plans[j]) +
                   " Should be: '" + JSON.stringify(allresults.results[0]) +
                   "' but Is: " + JSON.stringify(allresults.results[j]) + "'"
                  );
      }
    },
    
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test in detail that an index range can be used for an equality filter.
    ////////////////////////////////////////////////////////////////////////////////
    
    testRangeEquals: function () {
      var query = "FOR v IN " + colName + " FILTER v.a == 1 RETURN [v.a, v.b]";

      var XPresult;
      var QResults=[];
      var i, j;

      // the index we will compare to sorts by a & b, so we need to
      // re-sort the result here to accomplish similarity.
      QResults[0] = AQL_EXECUTE(query, { }, paramNone).json;

      // -> use-index-range alone.
      QResults[1] = AQL_EXECUTE(query, { }, paramIndexRange).json;
      XPresult    = AQL_EXPLAIN(query, { }, paramIndexRange);
      assertEqual([ secondRuleName ], removeAlwaysOnClusterRules(XPresult.plan.rules));
      // the sortnode and its calculation node should be there.
      hasCalculationNodes(XPresult, 2);

      // The IndexNode created by this rule should be more clever, it knows the ranges.

      for (i = 1; i < 2; i++) {
        assertTrue(isEqual(QResults[0].sort(sortArray), QResults[i].sort(sortArray)), "Result " + i + " is Equal?");
      }
      var allresults = getQueryMultiplePlansAndExecutions(query, {});
      for (j = 1; j < allresults.results.length; j++) {
          assertTrue(isEqual(allresults.results[0].json.sort(sortArray),
                             allresults.results[j].json.sort(sortArray)),
                   "while execution of '" + query +
                   "' this plan gave the wrong results: " + JSON.stringify(allresults.plans[j]) +
                   " Should be: '" + JSON.stringify(allresults.results[0]) +
                   "' but Is: " + JSON.stringify(allresults.results[j]) + "'"
                  );
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test in detail that an index range can be used for a less than filter
    ////////////////////////////////////////////////////////////////////////////////

    testRangeLessThan: function () {
      var query = "FOR v IN " + colName + " FILTER v.a < 5 RETURN [v.a, v.b]";

      var XPresult;
      var QResults=[];
      var j;

      // the index we will compare to sorts by a & b, so we need to
      // re-sort the result here to accomplish similarity.
      QResults[0] = AQL_EXECUTE(query, { }, paramNone).json.sort(sortArray);

      // -> use-index-range alone.
      QResults[1] = AQL_EXECUTE(query, { }, paramIndexRange).json.sort(sortArray);

      XPresult    = AQL_EXPLAIN(query, { }, paramIndexRange);
      assertEqual([ secondRuleName ], removeAlwaysOnClusterRules(XPresult.plan.rules));
      // the sortnode and its calculation node should be there.
      hasCalculationNodes(XPresult, 2);

      assertTrue(isEqual(QResults[0], QResults[1]), "Results are equal?");

      var allresults = getQueryMultiplePlansAndExecutions(query, {});
      for (j = 1; j < allresults.results.length; j++) {
        assertTrue(isEqual(allresults.results[0].json.sort(sortArray),
                           allresults.results[j].json.sort(sortArray)),
                   "while execution of '" + query +
                   "' this plan gave the wrong results: " + JSON.stringify(allresults.plans[j]) +
                   " Should be: '" + JSON.stringify(allresults.results[0]) +
                   "' but Is: " + JSON.stringify(allresults.results[j]) + "'"
                  );
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test in detail that an index range can be used for a greater than filter.
    ////////////////////////////////////////////////////////////////////////////////

    testRangeGreaterThan: function () {
      var query = "FOR v IN " + colName + " FILTER v.a > 5 RETURN [v.a, v.b]";
      var XPresult;
      var QResults=[];
      var j;

      // the index we will compare to sorts by a & b, so we need to
      // re-sort the result here to accomplish similarity.
      QResults[0] = AQL_EXECUTE(query, { }, paramNone).json.sort(sortArray);

      // -> use-index-range alone.
      QResults[1] = AQL_EXECUTE(query, { }, paramIndexRange).json.sort(sortArray);

      XPresult    = AQL_EXPLAIN(query, { }, paramIndexRange);
      assertEqual([ secondRuleName ], removeAlwaysOnClusterRules(XPresult.plan.rules));
      // the sortnode and its calculation node should be there.
      hasCalculationNodes(XPresult, 2);

      assertTrue(isEqual(QResults[0], QResults[1]), "Results are Equal?");

      var allresults = getQueryMultiplePlansAndExecutions(query, {});
      for (j = 1; j < allresults.results.length; j++) {
        assertTrue(isEqual(allresults.results[0].json.sort(sortArray),
                           allresults.results[j].json.sort(sortArray)),
                   "while execution of '" + query +
                   "' this plan gave the wrong results: " + JSON.stringify(allresults.plans[j]) +
                   " Should be: '" + JSON.stringify(allresults.results[0]) +
                   "' but Is: " + JSON.stringify(allresults.results[j]) + "'"
                  );
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test in detail that an index range can be used for an and combined 
    ///   greater than + less than filter spanning a range.
    ////////////////////////////////////////////////////////////////////////////////

    testRangeBandpass: function () {
      var query = "FOR v IN " + colName + " FILTER v.a > 4 && v.a < 10 RETURN [v.a, v.b]";
      var XPresult;
      var QResults=[];
      var j;

      // the index we will compare to sorts by a & b, so we need to
      // re-sort the result here to accomplish similarity.
      QResults[0] = AQL_EXECUTE(query, { }, paramNone).json.sort(sortArray);

      // -> use-index-range alone.
      QResults[1] = AQL_EXECUTE(query, { }, paramIndexRange).json.sort(sortArray);

      XPresult    = AQL_EXPLAIN(query, { }, paramIndexRange);
      assertEqual([ secondRuleName ], removeAlwaysOnClusterRules(XPresult.plan.rules));
      // the sortnode and its calculation node should be there.
      hasCalculationNodes(XPresult, 2);

      assertTrue(isEqual(QResults[0], QResults[1]), "Results are Equal?");

      var allresults = getQueryMultiplePlansAndExecutions(query, {});
      for (j = 1; j < allresults.results.length; j++) {
        assertTrue(isEqual(allresults.results[0].json.sort(sortArray),
                           allresults.results[j].json.sort(sortArray)),
                   "while execution of '" + query +
                   "' this plan gave the wrong results: " + JSON.stringify(allresults.plans[j]) +
                   " Should be: '" + JSON.stringify(allresults.results[0]) +
                   "' but Is: " + JSON.stringify(allresults.results[j]) + "'"
                  );
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test in detail that an index range can be used for an and combined 
    ///   greater than + less than filter spanning an empty range. This actually
    ///   recognizes the empty range and introduces a NoResultsNode but not an
    ///   IndexNode.
    ////////////////////////////////////////////////////////////////////////////////

    testRangeBandpassInvalid: function () {
      var query = "FOR v IN " + colName + " FILTER v.a > 7 && v.a < 4 RETURN [v.a, v.b]";
      var j;
      var XPresult;
      var QResults=[];

      // the index we will compare to sorts by a & b, so we need to re-sort
      // the result here to accomplish similarity.
      QResults[0] = AQL_EXECUTE(query, { }, paramNone).json.sort(sortArray);

      // -> use-indexes alone.
      QResults[1] = AQL_EXECUTE(query, { }, paramIndexRange).json;


      XPresult    = AQL_EXPLAIN(query, { }, paramIndexRange);
      assertEqual([ secondRuleName ], removeAlwaysOnClusterRules(XPresult.plan.rules));
      // the sortnode and its calculation node should be there.
      hasCalculationNodes(XPresult, 2);

      hasNoResultsNode(XPresult);
      hasNoIndexNode(XPresult);

      assertTrue(isEqual(QResults[0], QResults[1]), "Results are Equal?");
      var allresults = getQueryMultiplePlansAndExecutions(query, {});
      for (j = 1; j < allresults.results.length; j++) {
        assertTrue(isEqual(allresults.results[0],
                           allresults.results[j]),
                   "while execution of '" + query +
                   "' this plan gave the wrong results: " + JSON.stringify(allresults.plans[j]) +
                   " Should be: '" + JSON.stringify(allresults.results[0]) +
                   "' but Is: " + JSON.stringify(allresults.results[j]) + "'"
                  );
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test in detail that an index range can be used for an or combined 
    ///   greater than + less than filter spanning a range. 
    ////////////////////////////////////////////////////////////////////////////////

    testRangeBandstop: function () {
      var query = "FOR v IN " + colName + " FILTER v.a < 5 || v.a > 10 RETURN [v.a, v.b]";

      var XPresult;
      var QResults=[];

      // the index we will compare to sorts by a & b, so we need to
      // re-sort the result here to accomplish similarity.
      QResults[0] = AQL_EXECUTE(query, { }, paramNone).json.sort(sortArray);

      // -> use-index-range alone.
      QResults[1] = AQL_EXECUTE(query, { }, paramIndexRange).json.sort(sortArray);

      XPresult    = AQL_EXPLAIN(query, { }, paramIndexRange);
      assertEqual([ secondRuleName ], removeAlwaysOnClusterRules(XPresult.plan.rules));

      // the sortnode and its calculation node should be there.
      assertEqual(1, findExecutionNodes(XPresult, "IndexNode").length);

      assertTrue(isEqual(QResults[0], QResults[1]), "Results are equal?");
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test in detail that an index range can be used for an or combined 
    ///   greater than + less than filter spanning multiple ranges. 
    ////////////////////////////////////////////////////////////////////////////////

    testMultiRangeBandpass: function () {
      var query = "FOR v IN " + colName + " FILTER ((v.a > 3 && v.a < 5) || (v.a > 4 && v.a < 7)) RETURN [v.a, v.b]";

      var XPresult;
      var QResults=[];

      // the index we will compare to sorts by a & b, so we need to
      // re-sort the result here to accomplish similarity.
      QResults[0] = AQL_EXECUTE(query, { }, paramNone).json.sort(sortArray);

      // -> use-index-range alone.
      QResults[1] = AQL_EXECUTE(query, { }, paramIndexRange).json;

      XPresult    = AQL_EXPLAIN(query, { }, paramIndexRange);
      assertEqual(["use-indexes"], removeAlwaysOnClusterRules(XPresult.plan.rules));

      // the sortnode and its calculation node should be there.
      assertEqual(1, findExecutionNodes(XPresult, "IndexNode").length);
      hasCalculationNodes(XPresult, 2);

      /*
      // TODO: activate the following once OR is implemented
      // The IndexNode created by this rule should be more clever, it knows the ranges.
      var RAs = getRangeAttributes(XPresult);
      // FIXME constant bounds don't have a vType or value 
      //var first = getRangeAttribute(RAs[0], "v", "a", 1);
      //assertEqual(first.lowConst.bound.vType, "int", "Type is int");
      //assertEqual(first.lowConst.bound, 4, "proper value was set");
      //assertEqual(first.highConst.bound.vType, "int", "Type is int");
      //assertEqual(first.highConst.bound, 5, "proper value was set");
      assertTrue(isEqual(QResults[0], QResults[1]), "Results are Equal?");
      */
    },

    testSortAscEmptyCollection : function () {
      skiplist.truncate({ compact: false });
      assertEqual(0, skiplist.count());

      // should not crash
      var result = AQL_EXECUTE("FOR v IN " + colName + " SORT v.d ASC RETURN v");
      assertEqual([ ], result.json);
      
      var rules = AQL_EXPLAIN("FOR v IN " + colName + " SORT v.d ASC RETURN v").plan.rules;
      assertNotEqual(-1, rules.indexOf(ruleName));

      var nodes = AQL_EXPLAIN("FOR v IN " + colName + " SORT v.d ASC RETURN v").plan.nodes;
      var seen = false;
      nodes.forEach(function(node) {
        if (node.type === "IndexNode") {
          seen = true;
          assertFalse(node.reverse);
        }
      });
      assertTrue(seen);
    },

    testSortDescEmptyCollection : function () {
      skiplist.truncate({ compact: false });
      assertEqual(0, skiplist.count());

      // should not crash
      var result = AQL_EXECUTE("FOR v IN " + colName + " SORT v.d DESC RETURN v");
      assertEqual([ ], result.json);
      
      var rules = AQL_EXPLAIN("FOR v IN " + colName + " SORT v.d DESC RETURN v").plan.rules;
      assertNotEqual(-1, rules.indexOf(ruleName));
      
      var nodes = AQL_EXPLAIN("FOR v IN " + colName + " SORT v.d DESC RETURN v").plan.nodes;
      var seen = false;
      nodes.forEach(function(node) {
        if (node.type === "IndexNode") {
          seen = true;
          assertTrue(node.reverse);
        }
      });
      assertTrue(seen);
    },

    testSortAscWithFilter : function () {
      var query = "FOR v IN " + colName + " FILTER v.d == 123 SORT v.d ASC RETURN v";
      var rules = AQL_EXPLAIN(query).plan.rules;
      assertNotEqual(-1, rules.indexOf(ruleName));
      assertNotEqual(-1, rules.indexOf(secondRuleName));
      assertNotEqual(-1, rules.indexOf("remove-filter-covered-by-index"));

      var nodes = AQL_EXPLAIN(query).plan.nodes;
      var seen = false;
      nodes.forEach(function(node) {
        assertNotEqual("SortNode", node.type);
        if (node.type === "IndexNode") {
          seen = true;
          assertFalse(node.reverse);
        }
      });
      assertTrue(seen);
    },
    
    testSortAscWithFilterMulti : function () {
      var query = "FOR v IN " + colName + " FILTER v.a == 123 SORT v.b ASC RETURN v";
      var rules = AQL_EXPLAIN(query).plan.rules;
      assertNotEqual(-1, rules.indexOf(ruleName));
      assertNotEqual(-1, rules.indexOf(secondRuleName));
      assertNotEqual(-1, rules.indexOf("remove-filter-covered-by-index"));

      var nodes = AQL_EXPLAIN(query).plan.nodes;
      var seen = false;
      nodes.forEach(function(node) {
        assertNotEqual("SortNode", node.type);
        if (node.type === "IndexNode") {
          seen = true;
          assertFalse(node.reverse);
        }
      });
      assertTrue(seen);
    },
    
    testSortAscWithFilterNonConst : function () {
      var query = "FOR v IN " + colName + " FILTER v.d == NOOPT(123) SORT v.d ASC RETURN v";
      var rules = AQL_EXPLAIN(query).plan.rules;
      assertNotEqual(-1, rules.indexOf(ruleName));
      assertNotEqual(-1, rules.indexOf(secondRuleName));
      assertNotEqual(-1, rules.indexOf("remove-filter-covered-by-index"));

      var nodes = AQL_EXPLAIN(query).plan.nodes;
      var seen = false;
      nodes.forEach(function(node) {
        assertNotEqual("SortNode", node.type);
        if (node.type === "IndexNode") {
          seen = true;
          assertFalse(node.reverse);
        }
      });
      assertTrue(seen);
    },
    
    testSortAscWithFilterNonConstMulti : function () {
      var query = "FOR v IN " + colName + " FILTER v.a == NOOPT(123) SORT v.b ASC RETURN v";
      var rules = AQL_EXPLAIN(query).plan.rules;
      assertNotEqual(-1, rules.indexOf(ruleName));
      assertNotEqual(-1, rules.indexOf(secondRuleName));
      assertNotEqual(-1, rules.indexOf("remove-filter-covered-by-index"));

      var nodes = AQL_EXPLAIN(query).plan.nodes;
      var seen = false;
      nodes.forEach(function(node) {
        assertNotEqual("SortNode", node.type);
        if (node.type === "IndexNode") {
          seen = true;
          assertFalse(node.reverse);
        }
      });
      assertTrue(seen);
    },
    
    testSortDescWithFilter : function () {
      var query = "FOR v IN " + colName + " FILTER v.d == 123 SORT v.d DESC RETURN v";
      var rules = AQL_EXPLAIN(query).plan.rules;
      assertNotEqual(-1, rules.indexOf(ruleName));
      assertNotEqual(-1, rules.indexOf(secondRuleName));
      assertNotEqual(-1, rules.indexOf("remove-filter-covered-by-index"));

      var nodes = AQL_EXPLAIN(query).plan.nodes;
      var seen = false;
      nodes.forEach(function(node) {
        assertNotEqual("SortNode", node.type);
        if (node.type === "IndexNode") {
          seen = true;
          assertFalse(node.reverse); // forward or backward does not matter here
        }
      });
      assertTrue(seen);
    },
    
    testSortDescWithFilterMulti : function () {
      var query = "FOR v IN " + colName + " FILTER v.a == 123 SORT v.b DESC RETURN v";
      var rules = AQL_EXPLAIN(query).plan.rules;
      assertNotEqual(-1, rules.indexOf(ruleName));
      assertNotEqual(-1, rules.indexOf(secondRuleName));
      assertNotEqual(-1, rules.indexOf("remove-filter-covered-by-index"));

      var nodes = AQL_EXPLAIN(query).plan.nodes;
      var seen = false;
      nodes.forEach(function(node) {
        assertNotEqual("SortNode", node.type);
        if (node.type === "IndexNode") {
          seen = true;
          assertTrue(node.reverse); 
        }
      });
      assertTrue(seen);
    },
    
    testSortAscWithFilterSubquery : function () {
      const query = `FOR i IN [123] RETURN (FOR v IN ${colName} FILTER v.a == i SORT v.b ASC RETURN v)`;
      const plan = AQL_EXPLAIN(query, {}, {optimizer: {rules: ['-splice-subqueries']}}).plan;
      const rules = plan.rules;
      assertNotEqual(-1, rules.indexOf(ruleName));
      assertNotEqual(-1, rules.indexOf(secondRuleName));
      assertNotEqual(-1, rules.indexOf("remove-filter-covered-by-index"));
      assertEqual(-1, rules.indexOf("splice-subqueries"));

      const nodes = helper.findExecutionNodes(plan, "SubqueryNode")[0].subquery.nodes;
      // We expect no SortNode...
      assertFalse(nodes.some(node => node.type === 'SortNode'));
      const indexNodes = nodes.filter(node => node.type === 'IndexNode');
      // ...and one IndexNode...
      assertEqual(1, indexNodes.length);
      // ...which is not reversed.
      assertFalse(indexNodes[0].reverse);
    },

    testSortAscWithFilterSplicedSubquery : function () {
      const query = `FOR i IN [123] RETURN (FOR v IN ${colName} FILTER v.a == i SORT v.b ASC RETURN v)`;
      const plan = AQL_EXPLAIN(query, {}, {optimizer: {rules: ['+splice-subqueries']}}).plan;
      const rules = plan.rules;
      assertNotEqual(-1, rules.indexOf(ruleName));
      assertNotEqual(-1, rules.indexOf(secondRuleName));
      assertNotEqual(-1, rules.indexOf("remove-filter-covered-by-index"));
      assertNotEqual(-1, rules.indexOf("splice-subqueries"));

      const subqueryStartIdx = plan.nodes.findIndex(node => node.type === 'SubqueryStartNode');
      const subqueryEndIdx = plan.nodes.findIndex(node => node.type === 'SubqueryEndNode');

      const nodes = plan.nodes.slice(subqueryStartIdx+1, subqueryEndIdx);
      // We expect no SortNode...
      assertFalse(nodes.some(node => node.type === 'SortNode'));
      const indexNodes = nodes.filter(node => node.type === 'IndexNode');
      // ...and one IndexNode...
      assertEqual(1, indexNodes.length);
      // ...which is not reversed.
      assertFalse(indexNodes[0].reverse);
    },
    
    testSortDescWithFilterSubquery : function () {
      const query = `FOR i IN [123] RETURN (FOR v IN ${colName} FILTER v.a == i SORT v.b DESC RETURN v)`;
      const plan = AQL_EXPLAIN(query, {}, {optimizer: {rules: ['-splice-subqueries']}}).plan;
      const rules = plan.rules;
      assertNotEqual(-1, rules.indexOf(ruleName));
      assertNotEqual(-1, rules.indexOf(secondRuleName));
      assertNotEqual(-1, rules.indexOf("remove-filter-covered-by-index"));
      assertEqual(-1, rules.indexOf("splice-subqueries"));

      const nodes = helper.findExecutionNodes(plan, "SubqueryNode")[0].subquery.nodes;
      // We expect no SortNode...
      assertFalse(nodes.some(node => node.type === 'SortNode'));
      const indexNodes = nodes.filter(node => node.type === 'IndexNode');
      // ...and one IndexNode...
      assertTrue(indexNodes.length === 1);
      // ...which is reversed.
      assertTrue(indexNodes[0].reverse);
    },

    testSortDescWithFilterSplicedSubquery : function () {
      const query = `FOR i IN [123] RETURN (FOR v IN ${colName} FILTER v.a == i SORT v.b DESC RETURN v)`;
      const plan = AQL_EXPLAIN(query, {}, {optimizer: {rules: ['+splice-subqueries']}}).plan;
      const rules = plan.rules;
      assertNotEqual(-1, rules.indexOf(ruleName));
      assertNotEqual(-1, rules.indexOf(secondRuleName));
      assertNotEqual(-1, rules.indexOf("remove-filter-covered-by-index"));
      assertNotEqual(-1, rules.indexOf("splice-subqueries"));

      const subqueryStartIdx = plan.nodes.findIndex(node => node.type === 'SubqueryStartNode');
      const subqueryEndIdx = plan.nodes.findIndex(node => node.type === 'SubqueryEndNode');

      const nodes = plan.nodes.slice(subqueryStartIdx+1, subqueryEndIdx);
      // We expect no SortNode...
      assertFalse(nodes.some(node => node.type === 'SortNode'));
      const indexNodes = nodes.filter(node => node.type === 'IndexNode');
      // ...and one IndexNode...
      assertTrue(indexNodes.length === 1);
      // ...which is reversed.
      assertTrue(indexNodes[0].reverse);
    },

    testSortDescWithFilterNonConst : function () {
      var query = "FOR v IN " + colName + " FILTER v.d == NOOPT(123) SORT v.d DESC RETURN v";
      var rules = AQL_EXPLAIN(query).plan.rules;
      assertNotEqual(-1, rules.indexOf(ruleName));
      assertNotEqual(-1, rules.indexOf(secondRuleName));
      assertNotEqual(-1, rules.indexOf("remove-filter-covered-by-index"));

      var nodes = AQL_EXPLAIN(query).plan.nodes;
      var seen = false;
      nodes.forEach(function(node) {
        assertNotEqual("SortNode", node.type);
        if (node.type === "IndexNode") {
          seen = true;
          assertFalse(node.reverse); // forward or backward does not matter here
        }
      });
      assertTrue(seen);
    },
    
    testSortDescWithFilterNonConstMulti : function () {
      var query = "FOR v IN " + colName + " FILTER v.a == NOOPT(123) SORT v.b DESC RETURN v";
      var rules = AQL_EXPLAIN(query).plan.rules;
      assertNotEqual(-1, rules.indexOf(ruleName));
      assertNotEqual(-1, rules.indexOf(secondRuleName));
      assertNotEqual(-1, rules.indexOf("remove-filter-covered-by-index"));

      var nodes = AQL_EXPLAIN(query).plan.nodes;
      var seen = false;
      nodes.forEach(function(node) {
        assertNotEqual("SortNode", node.type);
        if (node.type === "IndexNode") {
          seen = true;
          assertTrue(node.reverse); 
        }
      });
      assertTrue(seen);
    },
    
    testSortModifyFilterCondition : function () {
      var query = "FOR v IN " + colName + " FILTER v.a == 123 SORT v.a, v.xxx RETURN v";
      var rules = AQL_EXPLAIN(query).plan.rules;
      assertEqual(-1, rules.indexOf(ruleName));
      assertNotEqual(-1, rules.indexOf(secondRuleName));
      assertNotEqual(-1, rules.indexOf("remove-filter-covered-by-index"));

      var nodes = AQL_EXPLAIN(query).plan.nodes;
      var seen = 0;
      nodes.forEach(function(node) {
        if (node.type === "IndexNode") {
          ++seen;
          assertFalse(node.reverse);
        } else if (node.type === "SortNode") {
          ++seen;
          assertEqual(2, node.elements.length);
        }
      });
      assertEqual(2, seen);
    },

    testSortOnSubAttributeAsc : function () {
      skiplist.ensureIndex({ type: "skiplist", fields: [ "foo.bar" ], unique: false });
      var query = "FOR v IN " + colName + " SORT v.foo.bar ASC RETURN v";
      var rules = AQL_EXPLAIN(query).plan.rules;
      assertNotEqual(-1, rules.indexOf(ruleName));

      var nodes = AQL_EXPLAIN(query).plan.nodes;
      var seen = false;
      nodes.forEach(function(node) {
        assertNotEqual("SortNode", node.type);
        if (node.type === "IndexNode") {
          assertEqual(1, node.indexes.length);
          assertEqual(["foo.bar"], node.indexes[0].fields);
          seen = true;
          assertFalse(node.reverse); 
        }
      });
      assertTrue(seen);
    },

    testSortOnSubAttributeDesc : function () {
      skiplist.ensureIndex({ type: "skiplist", fields: [ "foo.bar" ], unique: false });
      var query = "FOR v IN " + colName + " SORT v.foo.bar DESC RETURN v";
      var rules = AQL_EXPLAIN(query).plan.rules;
      assertNotEqual(-1, rules.indexOf(ruleName));

      var nodes = AQL_EXPLAIN(query).plan.nodes;
      var seen = false;
      nodes.forEach(function(node) {
        assertNotEqual("SortNode", node.type);
        if (node.type === "IndexNode") {
          assertEqual(1, node.indexes.length);
          assertEqual(["foo.bar"], node.indexes[0].fields);
          seen = true;
          assertTrue(node.reverse); 
        }
      });
      assertTrue(seen);
    },

    testSortOnNestedSubAttributeAsc : function () {
      skiplist.ensureIndex({ type: "skiplist", fields: [ "foo.bar.baz" ], unique: false });
      var query = "FOR v IN " + colName + " SORT v.foo.bar.baz ASC RETURN v";
      var rules = AQL_EXPLAIN(query).plan.rules;
      assertNotEqual(-1, rules.indexOf(ruleName));

      var nodes = AQL_EXPLAIN(query).plan.nodes;
      var seen = false;
      nodes.forEach(function(node) {
        assertNotEqual("SortNode", node.type);
        if (node.type === "IndexNode") {
          assertEqual(1, node.indexes.length);
          assertEqual(["foo.bar.baz"], node.indexes[0].fields);
          seen = true;
          assertFalse(node.reverse); 
        }
      });
      assertTrue(seen);
    },
    
    testSortOnNonIndexedSubAttributeAsc : function () {
      var query = "FOR v IN " + colName + " SORT v.foo.bar ASC RETURN v";
      var rules = AQL_EXPLAIN(query).plan.rules;
      assertEqual(-1, rules.indexOf(ruleName));
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(optimizerRuleTestSuite);

return jsunity.done();
