/*global require */

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
var loadTestRunner = require("loadtestrunner");
var repgen = require("reportgenerator");
var internal = require("internal");

var db = internal.db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////


var ruleName = "join";
var colName = "perf_" + ruleName.replace(/-/g, "_");

var theCollection;

var dbdApi = function (query, plan, bindVars) {
  db._query(query, bindVars).toArray();
  return {};
};


var setUp = function (options) {
  var loopto = options.dbcols;
  var contentmultiply = options.contentmultiply;

  var Content = Array(contentmultiply).join('abcdefghijklmnopqrstuvwxyz')

  internal.db._drop(colName);
  theCollection = internal.db._create(colName);
  var i, j;
  for (i = 1; i <= loopto; ++i) {
    theCollection.save({
      "Key" : i,
      "indexedKey" : i,
      "payload" : Content
    });
  }
  theCollection.ensureSkiplist("indexedKey");
};

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

var tearDown = function () {
  internal.db._drop(colName);
  require("internal").wait(0);
  theCollection = null;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Execute the query
////////////////////////////////////////////////////////////////////////////////

var testMethods = {
  ahuacatl : {executeQuery: dbdApi}
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Testcase: dump a full table
////////////////////////////////////////////////////////////////////////////////
var testFullRead = function (testParams, testMethodStr, testMethod) {
  var query = "FOR i IN " + colName + " RETURN i"; 
  return testMethod.executeQuery(query, {}, {});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Testcase: dump 10% of a table without using an index
////////////////////////////////////////////////////////////////////////////////
var testNonIndexedPartialRead = function (testParams, testMethodStr, testMethod, runOptions) {
  var tenPercent = (runOptions.dbcols / 10) * 9;
  var query = "FOR i IN " + colName + " FILTER i.Key > " + tenPercent + " RETURN i"; 
  return testMethod.executeQuery(query, {}, {});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Testcase: dump 10% of a table without using an index
////////////////////////////////////////////////////////////////////////////////
var testNonIndexedPartialReadCalcJS = function (testParams, testMethodStr, testMethod, runOptions) {
  var tenPercent = (runOptions.dbcols / 10) * 9;
  var query = "FOR i IN " + colName + " FILTER i.Key + 1 > " + tenPercent + " RETURN i"; 
  return testMethod.executeQuery(query, {}, {});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Testcase: dump a full table sorted by an unindexed key.
////////////////////////////////////////////////////////////////////////////////
var testNonIndexedFullSort = function (testParams, testMethodStr, testMethod) {
  var query = "FOR i IN " + colName + " SORT  i.Key RETURN i"; 
  return testMethod.executeQuery(query, {}, {});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Testcase: dump a full table sorted by an indexed key.
////////////////////////////////////////////////////////////////////////////////
var testIndexedFullSort = function (testParams, testMethodStr, testMethod) {
  var query = "FOR i IN " + colName + " SORT  i.indexedKey RETURN i"; 
  return testMethod.executeQuery(query, {}, {});
};


////////////////////////////////////////////////////////////////////////////////
/// @brief Testcase: dump a full table sorted by an indexed key.
////////////////////////////////////////////////////////////////////////////////
var testIndexedFullSortReverse = function (testParams, testMethodStr, testMethod) {
  var query = "FOR i IN " + colName + " SORT  i.indexedKey DESC RETURN i"; 
  return testMethod.executeQuery(query, {}, {});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Testcase: dump a full table sorted by an indexed key - use filter
/// so the old also has 
////////////////////////////////////////////////////////////////////////////////
var testIndexedFullSortFilter = function (testParams, testMethodStr, testMethod) {
  var query = "FOR i IN " + colName + " FILTER i.indexedKey > 0 SORT  i.indexedKey RETURN i"; 
  return testMethod.executeQuery(query, {}, {});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Testcase: dump a full table sorted by an indexed key - use filter
/// so the old also has 
////////////////////////////////////////////////////////////////////////////////
var testIndexedFullSortReverseFilter = function (testParams, testMethodStr, testMethod) {
  var query = "FOR i IN " + colName + " FILTER i.indexedKey > 0 SORT  i.indexedKey DESC RETURN i"; 
  return testMethod.executeQuery(query, {}, {});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Simple join testsuite
////////////////////////////////////////////////////////////////////////////////

var testSuite = [
  { name: "setup",    setUp: setUp, teardown: null, params: null, func: null},

  { name: "testFullRead", func: testFullRead},

  { name: "testNonIndexedPartialRead", func: testNonIndexedPartialRead},

  { name: "testNonIndexedPartialReadCalcJS", func: testNonIndexedPartialReadCalcJS},

  { name: "testNonIndexedFullSort", func: testNonIndexedFullSort},
  { name: "testIndexedFullSort", func: testIndexedFullSort},

  { name: "testIndexedFullSortReverse", func: testIndexedFullSortReverse},

  { name: "testIndexedFullSortFilter", func: testIndexedFullSortFilter},
  { name: "testIndexedFullSortReverseFilter", func: testIndexedFullSortReverseFilter},

  { name: "teardown",    setUp: null, teardown: tearDown, params: null, func: null}
];


////////////////////////////////////////////////////////////////////////////////
/// @brief execute suite with index and 25k entries in the collection
////////////////////////////////////////////////////////////////////////////////

var k, l;
for (k = 1; k < 22; k+=5) {
  for (l = 1; l < 22; l+=5) {
    var testOptions = {
      enableIndex: true,
      dbcols: 10000 * k,
      contentmultiply: l,
      runs: 5,   // number of runs for each test Has to be at least 3, else calculations will fail.
      strip: 1,   // how many min/max extreme values to ignore
      digits: 4   // result display digits
    };
    require("internal").print("Testrun with " + testOptions.dbcols + " documents in the collection by size: " + l);
    var ret = loadTestRunner.loadTestRunner(testSuite, testOptions, testMethods);
    //repgen.generatePerfReportJTL("join", ret);
  }
}
