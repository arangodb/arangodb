/*global require, exports, print */
'use strict';

var runTest = require('jsunity').runTest,
  _ = require('underscore'),
  internal = require('internal');

////////////////////////////////////////////////////////////////////////////////
/// @brief runs all jsunity tests
////////////////////////////////////////////////////////////////////////////////

function runJSUnityTests(tests) {
  var result = true;
  var allResults = [];
  var res;

  _.each(tests, function (file) {
    // find out whether we're on server or client...
    var runenvironment = "arangod";
    if (typeof(require('internal').arango) === 'object') {
      runenvironment = "arangosh";
    }

    if (result) {
      print("\n" + Date() + " " + runenvironment + ": Running JSUnity test from file '" + file + "'");
    } else {
      print("\n" + Date() + " " + runenvironment +
            ": Skipping JSUnity test from file '" + file + "' due to previous errors");
    }

    try {
      res = runTest(file);
      allResults.push(res);
      result = result && res.status;
    } catch (err) {
      print(runenvironment + ": cannot run test file '" + file + "': " + err);
      print(err.stack);
      print(err.message);
      result = false;
    }

    internal.wait(0); // force GC
  });
  require("fs").write("testresult.json", JSON.stringify(allResults));
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs all jsunity tests
////////////////////////////////////////////////////////////////////////////////

function runJasmineTests(testFiles, options) {
  var result = true;

  if (testFiles.length > 0) {
    print('\nRunning Jasmine Tests: ' + testFiles.join(', '));
    result = require('jasmine').executeTestSuite(testFiles, options);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs tests from command-line
////////////////////////////////////////////////////////////////////////////////

function runCommandLineTests(opts) {
  var result = true,
    options = opts || {},
    jasmineReportFormat = options.jasmineReportFormat || 'progress',
    unitTests = internal.unitTests(),
    isSpecRegEx = /.+spec\.js/,
    jasmine = _.filter(unitTests, isSpec),
    jsUnity = _.reject(unitTests, isSpec);

  function isSpec(unitTest) {
    return isSpecRegEx.test(unitTest);
  }

  result = runJSUnityTests(jsUnity) && runJasmineTests(jasmine, { format: jasmineReportFormat });

  internal.setUnitTestsResult(result);
}

exports.runCommandLineTests = runCommandLineTests;
