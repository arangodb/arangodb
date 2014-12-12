/*jshint strict: false, maxlen: 500 */
/*global require, assertEqual, assertTrue, AQL_EXPLAIN, AQL_EXECUTE */

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
var helper = require("org/arangodb/aql-helper");
var isEqual = helper.isEqual;
var findExecutionNodes = helper.findExecutionNodes;
var getQueryMultiplePlansAndExecutions = helper.getQueryMultiplePlansAndExecutions;
var removeAlwaysOnClusterRules = helper.removeAlwaysOnClusterRules;
var yaml=require("js-yaml");
// var x=function(a, b) {return yaml.safeDump(AQL_EXPLAIN(a, {}, b));};
var py=function(what) {require("internal").print( yaml.safeDump(what));};

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerRuleTestSuite() {
    var IndexRangeRule = "use-index-range";
    var FilterRemoveRule = "remove-filter-covered-by-index";
    var SortRemoveRule = "use-index-for-sort"; 
  var colName = "UnitTestsAqlOptimizer" + FilterRemoveRule.replace(/-/g, "_");
  var colNameOther = colName + "_XX";

  // various choices to control the optimizer: 
  var paramNone = { optimizer: { rules: [ "-all" ] } };
  var paramIndexRangeFilter   = { optimizer: { rules: [ "-all", "+" + IndexRangeRule,  "+" + FilterRemoveRule] } };
  var paramIndexRangeSortFilter   = { optimizer: { rules: [ "-all", "+" + IndexRangeRule,  "+" + FilterRemoveRule, "+" + SortRemoveRule]}/*, "allPlans": true*/ };
  var skiplist;
  var skiplist2;

  var hasNoFilterNode = function (plan) {
    assertEqual(findExecutionNodes(plan, "FilterNode").length, 0, "Has NO Filter Node");
  };
  var hasIndexRangeNode_WithRanges = function (plan, haveRanges) {
    var rn = findExecutionNodes(plan, "IndexRangeNode");
    assertEqual(rn.length, 1, "Has IndexRangeNode");
    assertTrue(rn[0].ranges.length > 0, "whether the IndexRangeNode ranges array is valid");
    if (haveRanges) {
      assertTrue(rn[0].ranges[0].length > 0, "Have IndexRangeNode with ranges");
    }
    else {
      assertEqual(rn[0].ranges[0].length, 0, "Have IndexRangeNode with NO ranges");
    }
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
      var loopto = 10;

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

      internal.db._drop(colNameOther);
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
    /// @brief test that rule has an effect on the FILTER and the SORT
    ////////////////////////////////////////////////////////////////////////////////

    testRuleHasEffect_Combine_Sort_Filter : function () {
      var allresults;
      var queries = [ 
        "FOR v IN " + colName + " FILTER v.a > 5 SORT v.a ASC RETURN [v.a]",
        "FOR v IN " + colName + " SORT v.a ASC FILTER v.a > 5 RETURN [v.a]",

        "FOR v IN " + colName + " FILTER v.d > 5 SORT v.d ASC RETURN [v.d]",
        "FOR v IN " + colName + " SORT v.d ASC FILTER v.d > 5 RETURN [v.d]",

        "FOR v IN " + colName + " SORT v.d FILTER v.d > 2 LIMIT 3 RETURN [v.d]  ",
        "FOR v IN " + colName + " FILTER v.d > 2 SORT v.d LIMIT 3 RETURN [v.d]  ",

        "FOR v IN " + colName + " FOR w IN 1..10 FILTER v.d > 2 SORT v.d RETURN [v.d]",
        "FOR v IN " + colName + " FOR w IN 1..10 SORT v.d FILTER v.d > 2 RETURN [v.d]",

        "FOR v IN " + colName + " LET x = (FOR w IN " + colNameOther + " RETURN w.f ) FILTER v.a > 4 SORT v.a RETURN [v.a]",
        "FOR v IN " + colName + " LET x = (FOR w IN " + colNameOther + " RETURN w.f ) SORT v.a FILTER v.a > 4 RETURN [v.a]"
      ];
      var QResults = [];
      var i = 0;
      queries.forEach(function(query) {
          var j, result;

	  result = AQL_EXPLAIN(query, { }, paramIndexRangeFilter);
          assertEqual([ IndexRangeRule, FilterRemoveRule ], 
                      removeAlwaysOnClusterRules(result.plan.rules), query);
	  hasNoFilterNode(result);
	  hasIndexRangeNode_WithRanges(result);

          result = AQL_EXPLAIN(query, { }, paramIndexRangeSortFilter);
          assertEqual([ IndexRangeRule, FilterRemoveRule, SortRemoveRule ], 
                      removeAlwaysOnClusterRules(result.plan.rules), query);
	  hasNoFilterNode(result);
	  hasIndexRangeNode_WithRanges(result);


          QResults[0] = AQL_EXECUTE(query, { }, paramNone).json;
          QResults[1] = AQL_EXECUTE(query, { }, paramIndexRangeFilter).json;
          QResults[2] = AQL_EXECUTE(query, { }, paramIndexRangeSortFilter).json;
	  
        assertTrue(isEqual(QResults[0], QResults[1]), "result " + i + " is equal?");
        assertTrue(isEqual(QResults[0], QResults[2]), "result " + i + " is equal?");

        allresults = getQueryMultiplePlansAndExecutions(query, {});
        for (j = 1; j < allresults.results.length; j++) {
            assertTrue(isEqual(allresults.results[0],
                               allresults.results[j]),
                       "whether the execution of '" + query +
                       "' this plan gave the wrong results: " + JSON.stringify(allresults.plans[j]) +
                       " Should be: '" + JSON.stringify(allresults.results[0]) +
                       "' but Is: " + JSON.stringify(allresults.results[j]) + "'"
                      );
        }
        i++;
      });

    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test that rule has an effect, the filter can be removed, but the sort
    /// is kept in place since the index can't fullfill all the sorting.
    ////////////////////////////////////////////////////////////////////////////////
    testRuleHasEffect_Combine_Sort_NO_Filter : function () {
      var allresults;
      var queries = [ 
        "FOR v IN " + colName + " FILTER v.a == 1 SORT v.a, v.c RETURN [v.a, v.b, v.c]",
        "FOR v IN " + colName + " LET x = (FOR w IN " 
          + colNameOther + " FILTER w.f == 1 SORT w.a, w.h RETURN  w.f ) SORT v.a , v.c FILTER v.a == 1RETURN [v.a, x]"
      ];
      var QResults = [];
      var i = 0;
      queries.forEach(function(query) {
          var j, result;

	  result = AQL_EXPLAIN(query, { }, paramIndexRangeFilter);
          assertEqual([ IndexRangeRule, FilterRemoveRule ], 
                      removeAlwaysOnClusterRules(result.plan.rules), query);
	  hasNoFilterNode(result);
	  hasIndexRangeNode_WithRanges(result);

          result = AQL_EXPLAIN(query, { }, paramIndexRangeSortFilter);
	  py(result);
          assertEqual([ IndexRangeRule, FilterRemoveRule ], 
                      removeAlwaysOnClusterRules(result.plan.rules), query);
	  hasNoFilterNode(result);
	  hasIndexRangeNode_WithRanges(result);


          QResults[0] = AQL_EXECUTE(query, { }, paramNone).json;
          QResults[1] = AQL_EXECUTE(query, { }, paramIndexRangeFilter).json;
          QResults[2] = AQL_EXECUTE(query, { }, paramIndexRangeSortFilter).json;
	  
        assertTrue(isEqual(QResults[0], QResults[1]), "result " + i + " is equal?");
        assertTrue(isEqual(QResults[0], QResults[2]), "result " + i + " is equal?");

        allresults = getQueryMultiplePlansAndExecutions(query, {});
        for (j = 1; j < allresults.results.length; j++) {
            assertTrue(isEqual(allresults.results[0],
                               allresults.results[j]),
                       "whether the execution of '" + query +
                       "' this plan gave the wrong results: " + JSON.stringify(allresults.plans[j]) +
                       " Should be: '" + JSON.stringify(allresults.results[0]) +
                       "' but Is: " + JSON.stringify(allresults.results[j]) + "'"
                      );
        }
        i++;
      });

    },

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
