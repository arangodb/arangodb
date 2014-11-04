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
  db._query(query, bindVars);
  return {};
};


var setUp = function (options) {
  var loopto = options.dbcols;

  internal.db._drop(colName);
  theCollection = internal.db._create(colName);
  var j;
  for (j = 1; j <= loopto; ++j) {
    theCollection.save(    { "value" : j});
  }
  if (options.enableIndex) {
    theCollection.ensureSkiplist("value");
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

var tearDown = function () {
  internal.db._drop(colName);
  theCollection = null;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Execute the query
////////////////////////////////////////////////////////////////////////////////

var testMethods = {
  ahuacatl : {executeQuery: dbdApi}
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Testcase: a Join on the same table.
////////////////////////////////////////////////////////////////////////////////
var testJoin = function (testParams, testMethodStr, testMethod, options) {
  var query = "FOR i IN " + colName + " FOR j IN " + colName + " FILTER i.value == j.value RETURN i.value"; 
  return testMethod.executeQuery(query, {}, {});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Simple join testsuite
////////////////////////////////////////////////////////////////////////////////

var joinTestSuite = [
  { name: "setup",    setUp: setUp, teardown: null, params: null, func: null},

  { name: "testJoin",          func: testJoin},

  { name: "teardown",    setUp: null, teardown: tearDown, params: null, func: null}
];


////////////////////////////////////////////////////////////////////////////////
/// @brief execute suite with index and 25k entries in the collection
////////////////////////////////////////////////////////////////////////////////

var testIndexedJoinOptions = {
  enableIndex: true,
  dbcols: 25000,
  runs: 5,   // number of runs for each test Has to be at least 3, else calculations will fail.
  strip: 1,   // how many min/max extreme values to ignore
  digits: 4   // result display digits
};

var ret = loadTestRunner.loadTestRunner(joinTestSuite, testIndexedJoinOptions, testMethods);
repgen.generatePerfReportJTL("join", ret);


////////////////////////////////////////////////////////////////////////////////
/// @brief execute suite without index and 2.5k entries in the collection
////////////////////////////////////////////////////////////////////////////////

var testNonIndexedJoinOptions = {
  enableIndex: false,
  dbcols: 2500,
  runs: 5,   // number of runs for each test Has to be at least 3, else calculations will fail.
  strip: 1,   // how many min/max extreme values to ignore
  digits: 4   // result display digits
};

var ret = loadTestRunner.loadTestRunner(joinTestSuite, testNonIndexedJoinOptions, testMethods);
repgen.generatePerfReportJTL("join", ret);
