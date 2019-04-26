/* jshint strict: false */
// //////////////////////////////////////////////////////////////////////////////
// / @brief JSUnity wrapper
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2010-2012 triagens GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Dr. Frank Celler
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var internal = require('internal');
var print = internal.print;
var fs = require('fs');
var console = require('console');

var TOTAL = 0;
var PASSED = 0;
var FAILED = 0;
var DURATION = 0;
var RESULTS = {};
var COMPLETE = {};

var SETUPS = 0;
var TEARDOWNS = 0;

var TOTALSETUPS = 0;
var TOTALTEARDOWNS = 0;

var jsUnity = require('./jsunity/jsunity').jsUnity;
var STARTTEST = 0.0;
var testFilter = "undefined";

function setTestFilter(filter) {
  testFilter = filter;
}

jsUnity.results.begin = function (total, suiteName) {
  print(Date() + ' Running ' + (suiteName || 'unnamed test suite'));
  print(' ' + total + ' test(s) found');
  print();
  RESULTS = {};

  STARTTEST = jsUnity.env.getDate();
};

jsUnity.results.pass = function (index, testName) {
  var newtime = jsUnity.env.getDate();

  RESULTS[testName].status = true;
  RESULTS[testName].duration = newtime - STARTTEST;

  print(newtime.toISOString() + internal.COLORS.COLOR_GREEN +  ' [PASSED] ' + testName + internal.COLORS.COLOR_RESET +
    ' in ' + STARTTEST.toISOString() + '+' + ((newtime - STARTTEST) / 1000).toFixed(3) + 's');

  STARTTEST = newtime;
};

jsUnity.results.fail = function (index, testName, message) {
  var newtime = jsUnity.env.getDate();

  RESULTS[testName].status = false;
  RESULTS[testName].message = message;
  RESULTS[testName].duration = newtime - STARTTEST;

  print(newtime.toISOString() + internal.COLORS.COLOR_RED + ' [FAILED] ' + testName + internal.COLORS.COLOR_RESET +
    ' in ' + STARTTEST.toISOString() + '+' + ((newtime - STARTTEST) / 1000).toFixed(3) + 's: ' +
    internal.COLORS.COLOR_RED + message + internal.COLORS.COLOR_RESET);

  STARTTEST = newtime;
};

jsUnity.results.end = function (passed, failed, duration) {
  print(' ' + passed + ' test(s) passed');
  print(' ' + ((failed > 0) ?
      internal.COLORS.COLOR_RED :
      internal.COLORS.COLOR_RESET) +
    failed + ' test(s) failed' + internal.COLORS.COLOR_RESET);
  print(' ' + duration + ' millisecond(s) elapsed');
  print();
};

jsUnity.results.beginSetUp = function(index, testName) {
  RESULTS[testName] = {};
  SETUPS = jsUnity.env.getDate();
};
jsUnity.results.endSetUp = function(index, testName) {
  RESULTS[testName].setUpDuration = jsUnity.env.getDate() - SETUPS;
  TOTALSETUPS += RESULTS[testName].setUpDuration;
};
jsUnity.results.beginTeardown = function(index, testName) {
  TEARDOWNS = jsUnity.env.getDate();
};
jsUnity.results.endTeardown = function(index, testName) {
  RESULTS[testName].tearDownDuration = jsUnity.env.getDate() - TEARDOWNS;
  TOTALTEARDOWNS += RESULTS[testName].tearDownDuration;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief runs a test with context
// //////////////////////////////////////////////////////////////////////////////

function Run (testsuite) {
  var suite = jsUnity.compile(testsuite);
  var definition = testsuite();

  var tests = [];
  var setUp;
  var tearDown;
  var setUpAll;
  var tearDownAll;

  if (definition.hasOwnProperty('setUp')) {
    setUp = definition.setUp;
  }

  if (definition.hasOwnProperty('tearDown')) {
    tearDown = definition.tearDown;
  }
  
  if (definition.hasOwnProperty('setUpAll')) {
    setUpAll = definition.setUpAll;
  }

  if (definition.hasOwnProperty('tearDownAll')) {
    tearDownAll = definition.tearDownAll;
  }

  var scope = {};
  scope.setUp = setUp;
  scope.tearDown = tearDown;
  scope.setUpAll = setUpAll;
  scope.tearDownAll = tearDownAll;

  for (var key in definition) {
    if ((testFilter !== "undefined" && testFilter !== undefined && testFilter !== null) && (key !== testFilter)) {
      // print(`test "${key}" doesn't match "${testFilter}", skipping`);
      continue;
    }
    if (key.indexOf('test') === 0) {
      var test = { name: key, fn: definition[key]};

      tests.push(test);
    } else if (key !== 'tearDown' && key !== 'setUp' && key !== 'tearDownAll' && key !== 'setUpAll') {
      console.error('unknown function: %s', key);
    }
  }
  if (tests.length === 0) {
    let err  = `There is no test in testsuite "${suite.suiteName}" or your filter "${testFilter}" didn't match on anything`;
    print(`${internal.COLORS.COLOR_RED}${err}${internal.COLORS.COLOR_RESET}`);
    let res = {
      suiteName: suite.suiteName,
      message: err,
      duration: 0,
      setUpDuration: 0,
      tearDownDuration: 0,
      passed: 0,
      status: false,
      failed: 1,
      total: 1
    };
    TOTAL += 1;
    FAILED += 2;
    COMPLETE[suite.suiteName] = res;
    return res;
  }

  suite = new jsUnity.TestSuite(suite.suiteName, scope);

  suite.tests = tests;
  suite.setUp = setUp;
  suite.tearDown = tearDown;
  suite.setUpAll = setUpAll;
  suite.tearDownAll = tearDownAll;

  var result = jsUnity.run(suite);
  TOTAL += result.total;
  PASSED += result.passed;
  FAILED += result.failed;
  DURATION += result.duration;
  
  
  let duplicates = [];
  for (var attrname in RESULTS) {
    if (RESULTS.hasOwnProperty(attrname)) {
      if (COMPLETE.hasOwnProperty(attrname)) {
        print("Duplicate testsuite '" + attrname + "' - already have: " + JSON.stringify(COMPLETE[attrname]) + "");
        duplicates.push(attrname);
      }
      COMPLETE[attrname] = RESULTS[attrname];
    }
  }
  if (duplicates.length !== 0) {
    throw("Duplicate testsuite '" + duplicates + "'");
  }
  return result;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief done with all tests
// //////////////////////////////////////////////////////////////////////////////

function Done (suiteName) {
  internal.printf('%d total, %d passed, %d failed, %d ms', TOTAL, PASSED, FAILED, DURATION);
  print();

  var ok = FAILED === 0;

  COMPLETE.duration = DURATION;
  COMPLETE.status = ok;
  COMPLETE.failed = FAILED;
  COMPLETE.total = TOTAL;
  COMPLETE.suiteName = suiteName;
  COMPLETE.totalSetUp = TOTALSETUPS;
  COMPLETE.totalTearDown = TOTALTEARDOWNS;

  TOTAL = 0;
  PASSED = 0;
  FAILED = 0;
  DURATION = 0;

  var ret = COMPLETE;
  COMPLETE = {};
  return ret;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief done with all tests
// //////////////////////////////////////////////////////////////////////////////

function WriteDone (suiteName) {
  var ret = Done(suiteName);
  let outPath = fs.join(require("internal").options()['temp.path'], 'testresult.json');
  fs.write(outPath, JSON.stringify(ret));
  return ret;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief runs a JSUnity test file
// //////////////////////////////////////////////////////////////////////////////

function RunTest (path, outputReply, filter) {
  var content;
  var f;

  content = fs.read(path);

  content = `(function(){ require('jsunity').jsUnity.attachAssertions(); return (function() { require('jsunity').setTestFilter(${JSON.stringify(filter)});  ${content} }());
});`;
  f = internal.executeScript(content, undefined, path);

  if (f === undefined) {
    throw 'cannot create context function';
  }

  var rc = f(path);
  if (outputReply === true) {
    return rc;
  } else {
    return rc.status;
  }
}

exports.setTestFilter = setTestFilter;
exports.jsUnity = jsUnity;
exports.run = Run;
exports.done = Done;
exports.writeDone = WriteDone;
exports.runTest = RunTest;
