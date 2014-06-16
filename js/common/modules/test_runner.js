/*jslint indent: 2, nomen: true, maxlen: 120, regexp: true, todo: true */
/*global module, require, exports, print */
var RunTest = require('jsunity').runTest;
var _ = require("underscore");
var internal = require('internal');
var fs = require('fs');

////////////////////////////////////////////////////////////////////////////////
/// @brief runs all jsunity tests
////////////////////////////////////////////////////////////////////////////////

function RunJSUnityTests (tests) {
  var result = true,
    i,
    file,
    ok;

  for (i = 0;  i < tests.length;  ++i) {
    file = tests[i];
    print();
    print("running tests from file '" + file + "'");

    try {
      ok = RunTest(file);
      result = result && ok;
    }
    catch (err) {
      print("cannot run test file '" + file + "': " + err);
      print(err.stack);
      result = false;
    }

    internal.wait(0); // force GC
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs all jsunity tests
////////////////////////////////////////////////////////////////////////////////

function RunJasmineTests (testFiles) {
  var result = true;

  if (testFiles.length > 0) {
    var tests = _.map(testFiles, function (x) { return fs.read(x); }),
      jasmine = require('jasmine');

    print('\nRunning Jasmine Tests: ' + testFiles.join(', '));
    result = jasmine.executeTestSuite(tests);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs tests from command-line
////////////////////////////////////////////////////////////////////////////////

function runCommandLineTests () {
  var result = true,
    unitTests = internal.unitTests(),
    isSpecRegEx = /.+spec.js/,
    isSpec = function(unitTest) {
      return isSpecRegEx.test(unitTest);
    },
    jasmine = _.filter(unitTests, isSpec),
    jsUnity = _.reject(unitTests, isSpec);

  result = RunJSUnityTests(jsUnity) && RunJasmineTests(jasmine);

  internal.setUnitTestsResult(result);
}

exports.runCommandLineTests = runCommandLineTests;
