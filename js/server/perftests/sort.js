/*global AQL_EXECUTE */

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
var exports;
var db = internal.db;
//var helper = require("@arangodb/aql-helper");
//var PY = function (plan) { require("internal").print(require("js-yaml").safeDump(plan));};
////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////


var ruleName = "sort";
//  var secondRuleName = "use-index-range";
//  var removeCalculationNodes = "remove-unnecessary-calculations-2";
var colName = "perf_" + ruleName.replace(/-/g, "_");
var colNameOther = colName + "_XX";
//var paramNone = { optimizer: { rules: [ "-all" ] } };
var paramProfile = {  profile : true };

// various choices to control the optimizer: 
/*
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
*/
var skiplist;
var skiplist2;

var oldApi = function (query, plan, bindVars) {
  db._query(query, bindVars);
  return {};
};
/*
var newApi = function (query, plan, bindVars) {
  var ret = AQL_EXECUTE(query, bindVars, paramProfile);
  return ret.profile;
};
*/
/*
var newApiUnoptimized = function (query, plan, bindVars) {
  AQL_EXECUTE(query, bindVars, paramNone);
  return {};
};
var newApiUnoptimized = function (query, plan, bindVars) {
  return {};
};
*/

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
// Datastructure: 
//  - double index on (a,b)/(f,g) for tests with these
//  - single column index on d/j to test sort behavior without sub-columns
//  - non-indexed columns c/h to sort without indices.
//  - non-skiplist indexed columns e/j to check whether its not selecting them.
//  - join column 'joinme' to intersect both tables.
////////////////////////////////////////////////////////////////////////////////

var setUp = function (options) {
  var loopto = options.dbcols;

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
//  skiplist.ensureIndex({ type: "hash", fields: [ "c" ], unique: false });
  if (skiplist.count() !== ((loopto*loopto) + 2*loopto)) {
    throw "1: not all planned entries made it to disk, bailing out. Expecting: " + ((loopto*loopto) + 2*loopto) + "got: " + skiplist2.count();
  }
    
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
  if (skiplist2.count() !== ((loopto*loopto) + 2*loopto)) {
    throw "2: not all planned entries made it to disk, bailing out. Expecting: " + ((loopto*loopto) + 2*loopto) + "got: " + skiplist2.count();
  }
//  skiplist2.ensureIndex({ type: "hash", fields: [ "h" ], unique: false });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

var tearDown = function () {
  internal.db._drop(colName);
  internal.db._drop(colNameOther);
  skiplist = null;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has no effect
////////////////////////////////////////////////////////////////////////////////


var testRuleNoEffect1 = function (testParams, testMethodStr, testMethod) {
  var query = "FOR v IN " + colName + " SORT v.c RETURN [v.a, v.b]";

  return testMethod.executeQuery(query, {}, {});
};
var testRuleNoEffect2 = function (testParams, testMethodStr, testMethod) {
  var query =  "FOR v IN " + colName + " SORT v.a DESC RETURN [v.a, v.b]";

  return testMethod.executeQuery(query, {}, {});
};
var testRuleNoEffect3 = function (testParams, testMethodStr, testMethod) {
  var query = "FOR v IN " + colName + " SORT v.b, v.a  RETURN [v.a]";

  return testMethod.executeQuery(query, {}, {});
};
var testRuleNoEffect4 = function (testParams, testMethodStr, testMethod) {
  var query =  "FOR v IN " + colName + " SORT v.c RETURN [v.a, v.b]";

  return testMethod.executeQuery(query, {}, {});
};
var testRuleNoEffect5 = function (testParams, testMethodStr, testMethod) {
  var query = "FOR v IN " + colName + " SORT CONCAT(TO_STRING(v.a), \"lol\") RETURN [v.a]";

  return testMethod.executeQuery(query, {}, {});
};


////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has an effect
////////////////////////////////////////////////////////////////////////////////
var testRuleHasEffect1 = function (testParams, testMethodStr, testMethod) {
  var query = "FOR v IN " + colName + " SORT v.d DESC RETURN [v.d]";

  return testMethod.executeQuery(query, {}, {});
};
var testRuleHasEffect2 = function (testParams, testMethodStr, testMethod) {
  var query = "FOR v IN " + colName + " SORT v.d ASC RETURN [v.d]";

  return testMethod.executeQuery(query, {}, {});
};
var testRuleHasEffect3 = function (testParams, testMethodStr, testMethod) {
  var query = "FOR v IN " + colName + " SORT v.d FILTER v.a > 2 LIMIT 3 RETURN [v.d]  ";

  return testMethod.executeQuery(query, {}, {});
};
var testRuleHasEffect4 = function (testParams, testMethodStr, testMethod) {
  var query = "FOR v IN " + colName + " FOR w IN 1..10 SORT v.d RETURN [v.d]";

  return testMethod.executeQuery(query, {}, {});
};
var testRuleHasEffect5 = function (testParams, testMethodStr, testMethod) {
  var query = "FOR v IN " + colName + " LET x = (FOR w IN " + colNameOther + " RETURN w.f ) SORT v.a RETURN [v.a]";

  return testMethod.executeQuery(query, {}, {});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has an effect, but the sort is kept in place since 
//   the index can't fullfill all the sorting.
////////////////////////////////////////////////////////////////////////////////
var testRuleHasEffectButSortsStill1 = function (testParams, testMethodStr, testMethod) {
  var query = "FOR v IN " + colName + " FILTER v.a == 1 SORT v.a, v.c RETURN [v.a, v.b, v.c]";

  return testMethod.executeQuery(query, {}, {});
};

var testRuleHasEffectButSortsStill2 = function (testParams, testMethodStr, testMethod) {
  var query = "FOR v IN " + colName + " LET x = (FOR w IN " 
    + colNameOther + " SORT w.j, w.h RETURN  w.f ) SORT v.a RETURN [v.a]";

  return testMethod.executeQuery(query, {}, {});
};



////////////////////////////////////////////////////////////////////////////////
/// @brief this sort is replaceable by an index.
////////////////////////////////////////////////////////////////////////////////
var testSortIndexable = function (testParams, testMethodStr, testMethod) {
  var query = "FOR v IN " + colName + " SORT v.a RETURN [v.a, v.b]";

  return testMethod.executeQuery(query, {}, {});
};


////////////////////////////////////////////////////////////////////////////////
/// @brief test in detail that this rule has an effect, but the sort is kept in
//    place since the index can't fullfill all of the sorting criteria.
////////////////////////////////////////////////////////////////////////////////
var testSortMoreThanIndexed = function (testParams, testMethodStr, testMethod) {
  var query = "FOR v IN " + colName + " FILTER v.a == 1 SORT v.a, v.c RETURN [v.a, v.b, v.c]";
  // no index can be used for v.c -> sort has to remain in place!

  return testMethod.executeQuery(query, {}, {});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief test in detail that an index range fullfills everything the sort does, 
//   and thus the sort is removed.
////////////////////////////////////////////////////////////////////////////////
var testRangeSuperseedsSort = function (testParams, testMethodStr, testMethod) {
  var query = "FOR v IN " + colName + " FILTER v.a == 1 SORT v.a RETURN [v.a, v.b, v.c]";

  return testMethod.executeQuery(query, {}, {});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief test in detail that an index range fullfills everything the sort does, 
//   and thus the sort is removed; multi-dimensional indexes are utilized.
////////////////////////////////////////////////////////////////////////////////
var testRangeSuperseedsSort2 = function (testParams, testMethodStr, testMethod) {
  var query = "FOR v IN " + colName + " FILTER v.a == 1 SORT v.a, v.b RETURN [v.a, v.b, v.c]";

  return testMethod.executeQuery(query, {}, {});
};


////////////////////////////////////////////////////////////////////////////////
/// @brief test in detail that an index range fullfills everything the sort does, 
//   and thus the sort is removed; multi-dimensional indexes are utilized.
////////////////////////////////////////////////////////////////////////////////
var testJoinIndexed = function (testParams, testMethodStr, testMethod) {
  var query = "FOR v IN " + colName + " FOR w in " + colNameOther + " FILTER v.a == w.f RETURN [v.a]";

  return testMethod.executeQuery(query, {}, {});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief test in detail that an index range fullfills everything the sort does, 
//   and thus the sort is removed; multi-dimensional indexes are utilized.
////////////////////////////////////////////////////////////////////////////////
var testJoinNonIndexed = function (testParams, testMethodStr, testMethod) {
  var query = "FOR v IN " + colName + " FOR w in " + colNameOther + " FILTER v.joinme == w.joinme RETURN [v.joinme]";

  return testMethod.executeQuery(query, {}, {});
};



////////////////////////////////////////////////////////////////////////////////
/// @brief test in detail that an index range can be used for an equality filter.
////////////////////////////////////////////////////////////////////////////////
var testRangeEquals = function (testParams, testMethodStr, testMethod) {
  var query = "FOR v IN " + colName + " FILTER v.a == 1 RETURN [v.a, v.b]";
  return testMethod.executeQuery(query, {}, {});
};


////////////////////////////////////////////////////////////////////////////////
/// @brief test in detail that an index range can be used for a less than filter.
////////////////////////////////////////////////////////////////////////////////
var testRangeLessThan = function (testParams, testMethodStr, testMethod) {
  var query = "FOR v IN " + colName + " FILTER v.a < 5 RETURN [v.a, v.b]";
  return testMethod.executeQuery(query, {}, {});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief test in detail that an index range can be used for a greater than filter.
////////////////////////////////////////////////////////////////////////////////
var testRangeGreaterThan = function (testParams, testMethodStr, testMethod) {
  var query = "FOR v IN " + colName + " FILTER v.a > 5 RETURN [v.a, v.b]";
  return testMethod.executeQuery(query, {}, {});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief test in detail that an index range can be used for an and combined 
///   greater than + less than filter spanning a range.
////////////////////////////////////////////////////////////////////////////////
var testRangeBandpass = function (testParams, testMethodStr, testMethod) {
  var query = "FOR v IN " + colName + " FILTER v.a > 4 && v.a < 10 RETURN [v.a, v.b]";
  return testMethod.executeQuery(query, {}, {});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief test in detail that an index range can be used for an and combined 
///   greater than + less than filter spanning an empty range. This actually
///   recognizes the empty range and introduces a NoResultsNode but not an
///   IndexRangeNode.
////////////////////////////////////////////////////////////////////////////////

var testRangeBandpassInvalid = function (testParams, testMethodStr, testMethod) {
  var query = "FOR v IN " + colName + " FILTER v.a > 7 && v.a < 4 RETURN [v.a, v.b]";
  return testMethod.executeQuery(query, {}, {});
};


////////////////////////////////////////////////////////////////////////////////
/// @brief test in detail that an index range can be used for an or combined 
///   greater than + less than filter spanning a range. TODO: doesn't work now.
////////////////////////////////////////////////////////////////////////////////
var testRangeBandstop = function (testParams, testMethodStr, testMethod) {
  var query = "FOR v IN " + colName + " FILTER v.a < 5 || v.a > 10 RETURN [v.a, v.b]";
  return testMethod.executeQuery(query, {}, {});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief test in detail that an index range can be used for an or combined 
///   greater than + less than filter spanning multiple ranges. TODO: doesn't work now.
////////////////////////////////////////////////////////////////////////////////

var testMultiRangeBandpass = function (testParams, testMethodStr, testMethod) {
  var query = "FOR v IN " + colName +
    " FILTER ((v.a > 3 && v.a < 5) || (v.a > 4 && v.a < 7)) RETURN [v.a, v.b]";
  return testMethod.executeQuery(query, {}, {});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////


var testOptions = {
  dbcols: 500,
  runs: 5,   // number of runs for each test Has to be at least 3, else calculations will fail.
  strip: 1,   // how many min/max extreme values to ignore
  digits: 4   // result display digits
};

var testMethods = {
  ahuacatl : {executeQuery: oldApi}
/*
  aql2Optimized : {executeQuery: newApi}
  aql2UnOptimized : {executeQuery: newApiUnoptimized},
  aql2Explain : {executeQuery: newApiUnoptimized, forceRuns: 1}
  
  */

};

var optimizerRuleTestSuite = [
  { name: "setup",    setUp: setUp, teardown: null, params: null, func: null},

  { name: "RuleNoEffect1",                func: testRuleNoEffect1},
  { name: "RuleNoEffect2",                func: testRuleNoEffect2},
  { name: "RuleNoEffect3",                func: testRuleNoEffect3},
  { name: "RuleNoEffect4",                func: testRuleNoEffect4},
  { name: "RuleNoEffect5",                func: testRuleNoEffect5},

  { name: "RuleHasEffect1",               func: testRuleHasEffect1},
  { name: "RuleHasEffect2",               func: testRuleHasEffect2},
  { name: "RuleHasEffect3",               func: testRuleHasEffect3},
  { name: "RuleHasEffect4",               func: testRuleHasEffect4},
  { name: "RuleHasEffect5",               func: testRuleHasEffect5},

  { name: "RuleHasEffectButSortsStill1",  func: testRuleHasEffectButSortsStill1},
  { name: "RuleHasEffectButSortsStill2",  func: testRuleHasEffectButSortsStill2},

  { name: "SortIndexable",               func: testSortIndexable},
  { name: "SortMoreThanIndexed",         func: testSortMoreThanIndexed},
  { name: "RangeSuperseedsSort",         func: testRangeSuperseedsSort},
  { name: "RangeSuperseedsSort2",        func: testRangeSuperseedsSort2},
  { name: "RangeEquals",                 func: testRangeEquals},
  { name: "RangeLessThan",               func: testRangeLessThan},
  { name: "RangeGreaterThan",            func: testRangeGreaterThan},
  { name: "RangeBandpass",               func: testRangeBandpass},
  { name: "RangeBandpassInvalid",        func: testRangeBandpassInvalid},
  { name: "RangeBandstop",               func: testRangeBandstop},

  { name: "MultiRangeBandpass",          func: testMultiRangeBandpass},


  { name: "teardown",    setUp: null, teardown: tearDown, params: null, func: null}
];



var loadTestRunner = require("loadtestrunner");
var repgen = require("reportgenerator");
//db=require("internal").db;
//skiplist  = db[colName].load();
//skiplist2 = db[colNameOther].load();

/*
var ret = loadTestRunner.loadTestRunner(optimizerRuleTestSuite, testOptions, testMethods);
//require("internal").print(JSON.stringify(ret));
repgen.generatePerfReportJTL("sort", ret);
*/

var testJoinOptions = {
  dbcols: 50,
  runs: 5,   // number of runs for each test Has to be at least 3, else calculations will fail.
  strip: 1,   // how many min/max extreme values to ignore
  digits: 4   // result display digits
};

var joinTestSuite = [
  { name: "setup",    setUp: setUp, teardown: null, params: null, func: null},

  { name: "testJoinIndexed",             func: testJoinIndexed},
  { name: "testJoinNonIndexed",          func: testJoinNonIndexed},

  { name: "teardown",    setUp: null, teardown: tearDown, params: null, func: null}
];


ret = loadTestRunner.loadTestRunner(joinTestSuite, testJoinOptions, testMethods);
//require("internal").print(JSON.stringify(ret));
repgen.generatePerfReportJTL("join", ret);


return ret;
