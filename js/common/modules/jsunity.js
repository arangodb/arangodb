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
var ENDTEST = 0.0;
var STARTSUITE = 0.0;
var ENDTEARDOWN = 0.0;
var testFilter = "undefined";
var currentSuiteName = "undefined";
var testCount = 0;
var startMessage = "";

function setTestFilter(filter) {
  testFilter = filter;
}

jsUnity.results.begin = function (total, suiteName) {
  if (testCount > 0)
  {
    print();
    testCount = 0;
  }
  currentSuiteName = suiteName;
  startMessage = " [------------] " + total + " tests from " + (suiteName || 'unnamed test suite');
  RESULTS = {};

  STARTTEST = jsUnity.env.getDate();
};

jsUnity.results.pass = function (index, testName) {
  var newtime = jsUnity.env.getDate();

  RESULTS[testName].status = true;
  RESULTS[testName].duration = (ENDTEST - STARTTEST);

  print(newtime.toISOString() + internal.COLORS.COLOR_GREEN +  ' [     PASSED ] ' +
       testName + internal.COLORS.COLOR_RESET +
       ' (setUp: ' + RESULTS[testName].setUpDuration + 'ms,' +
       ' test: ' + RESULTS[testName].duration + 'ms,' +
       ' tearDown: ' + RESULTS[testName].tearDownDuration + 'ms)');

  STARTTEST = newtime;

  ++testCount;
};

jsUnity.results.fail = function (index, testName, message) {
  var newtime = jsUnity.env.getDate();

  ++testCount;

  if (RESULTS[testName] === undefined)
  {
    if (testCount === 1)
    {
      print(newtime.toISOString() + internal.COLORS.COLOR_RED + " [   FAILED   ] " + currentSuiteName +
           internal.COLORS.COLOR_RESET + " (setUpAll: " + (jsUnity.env.getDate() - STARTTEST) + "ms)");

      ENDTEST = newtime;
    }
    print(internal.COLORS.COLOR_RED + message + internal.COLORS.COLOR_RESET);
    if (RESULTS.hasOwnProperty('message')) {
      RESULTS['message'] += "\n" + currentSuiteName + " - failed at: " + message;
    } else {
      RESULTS['message'] = currentSuiteName + " - failed at: " + message;
    }
    return;
  }

  RESULTS[testName].status = false;
  RESULTS[testName].message = message;
  RESULTS[testName].duration = (ENDTEST - STARTTEST);

  if (RESULTS[testName].setUpDuration === undefined)
  {
    RESULTS[testName].setUpDuration = newtime - SETUPS;
    RESULTS[testName].duration = 0;
  }

  print(newtime.toISOString() + internal.COLORS.COLOR_RED + " [   FAILED   ] " +
       testName + internal.COLORS.COLOR_RESET +
       ' (setUp: ' + RESULTS[testName].setUpDuration + 'ms,' +
       ' test: ' + RESULTS[testName].duration + 'ms,' +
       ' tearDown: ' +  (newtime - ENDTEST) + 'ms)\n' +
       internal.COLORS.COLOR_RED + message + internal.COLORS.COLOR_RESET);

  STARTTEST = newtime;
};

jsUnity.results.end = function (passed, failed, duration) {
  print(jsUnity.env.getDate().toISOString() +
       ((failed > 0) ? internal.COLORS.COLOR_RED : internal.COLORS.COLOR_GREEN) + " [------------] " +
       (passed + failed) + " tests from " + currentSuiteName + " ran" + internal.COLORS.COLOR_RESET +
       " (tearDownAll: " + (jsUnity.env.getDate() - ENDTEST) + "ms)");
  print(jsUnity.env.getDate().toISOString() + internal.COLORS.COLOR_GREEN +
       " [   PASSED   ] " + passed + " tests." + internal.COLORS.COLOR_RESET);
  if(failed > 0)
  {
    print(jsUnity.env.getDate().toISOString() + internal.COLORS.COLOR_RED +
        " [   FAILED   ] " + failed + ' tests.' + internal.COLORS.COLOR_RESET);
  }
};

jsUnity.results.beginSetUpAll = function(index) {
  SETUPS = jsUnity.env.getDate();
};

jsUnity.results.endSetUpAll = function(index) {
  RESULTS.setUpAllDuration = jsUnity.env.getDate() - SETUPS;
  TOTALSETUPS += RESULTS.setUpAllDuration;
};

jsUnity.results.beginSetUp = function(index, testName) {
  if (testCount === 0)
  {
    print(STARTTEST.toISOString() + internal.COLORS.COLOR_GREEN +
         startMessage + internal.COLORS.COLOR_RESET + ' (setUpAll: ' +
         (jsUnity.env.getDate() - STARTTEST) + 'ms)' + internal.COLORS.COLOR_RESET);
  }
  RESULTS[testName] = {};
  SETUPS = jsUnity.env.getDate();
  print(jsUnity.env.getDate().toISOString() + internal.COLORS.COLOR_GREEN + ' [ RUN        ] ' + testName + internal.COLORS.COLOR_RESET);
};

jsUnity.results.endSetUp = function(index, testName) {
  RESULTS[testName].setUpDuration = jsUnity.env.getDate() - SETUPS;
  TOTALSETUPS += RESULTS[testName].setUpDuration;

  STARTTEST = jsUnity.env.getDate();
};

jsUnity.results.beginTeardown = function(index, testName) {
  TEARDOWNS = jsUnity.env.getDate();

  ENDTEST = jsUnity.env.getDate();
};

jsUnity.results.endTeardown = function(index, testName) {
  RESULTS[testName].tearDownDuration = jsUnity.env.getDate() - TEARDOWNS;
  TOTALTEARDOWNS += RESULTS[testName].tearDownDuration;
};

jsUnity.results.beginTeardownAll = function(index) {
  TEARDOWNS = jsUnity.env.getDate();
};

jsUnity.results.endTeardownAll = function(index) {
  RESULTS.teardownAllDuration = jsUnity.env.getDate() - TEARDOWNS;
  TOTALTEARDOWNS += RESULTS.teardownAllDuration;
};

function MatchesTestFilter(key) {
  if (testFilter === "undefined" || testFilter === undefined || testFilter === null) {
    return true;
  }
  if (typeof testFilter === 'string') {
    return key === testFilter;
  }
  if (Array.isArray(testFilter)) {
    return testFilter.includes(key);
  }
  return false;
}
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
  let nonMatchedTests = [];

  for (var key in definition) {
    if (key.indexOf('test') === 0) {
      if (!MatchesTestFilter(key)) {
        // print(`test "${key}" doesn't match "${testFilter}", skipping`);
        nonMatchedTests.push(key);
        continue;
      }

      var test = { name: key, fn: definition[key]};

      tests.push(test);
    } else if (key !== 'tearDown' && key !== 'setUp' && key !== 'tearDownAll' && key !== 'setUpAll') {
      console.error('unknown function: %s', key);
    }
  }
  if (tests.length === 0) {
    let err  = `There is no test in testsuite "${suite.suiteName}" or your filter "${testFilter}" didn't match on anything. We have: [${nonMatchedTests}]`;
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
    if (typeof(RESULTS[attrname]) === 'number') {
      if (!COMPLETE.hasOwnProperty(attrname)) {
        COMPLETE[attrname] = { };
      }
      COMPLETE[attrname][suite.suiteName] = RESULTS[attrname];
    } else if (RESULTS.hasOwnProperty(attrname)) {
      if (COMPLETE.hasOwnProperty(attrname)) {
        if (attrname === 'message') {
          COMPLETE[attrname] += "\n\n" + RESULTS[attrname];
          continue;
        }
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
  let newtime = jsUnity.env.getDate();

  var ok = FAILED === 0;

  print(newtime.toISOString() + (ok ? internal.COLORS.COLOR_GREEN : internal.COLORS.COLOR_RED) +
       " [============] " + "Ran: " + TOTAL + " tests (" + PASSED + " passed, " + FAILED + " failed)" +
       internal.COLORS.COLOR_RESET+ " (" + DURATION + "ms total)");
  print();

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
  // re-reset our globlas, on module loading may be cached.
  TOTAL = 0;
  PASSED = 0;
  FAILED = 0;
  DURATION = 0;
  RESULTS = {};
  COMPLETE = {};
  
  SETUPS = 0;
  TEARDOWNS = 0;
  
  TOTALSETUPS = 0;
  TOTALTEARDOWNS = 0;
  STARTTEST = 0.0;
  ENDTEST = 0.0;
  STARTSUITE = 0.0;
  ENDTEARDOWN = 0.0;
  STARTTEST = 0.0;
  ENDTEST = 0.0;
  STARTSUITE = 0.0;
  ENDTEARDOWN = 0.0;

  var content;
  var f;

  content = fs.read(path);

  content = `(function(){ require('jsunity').jsUnity.attachAssertions(); return (function() { require('jsunity').setTestFilter(${JSON.stringify(filter)}); const runSetup = false; const getOptions = false; ${content} }());
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
