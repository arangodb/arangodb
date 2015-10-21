/*global print */
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
  var failed = [];
  var res;

  // find out whether we're on server or client...
  var runenvironment = "arangod";
  if (typeof(require('internal').arango) === 'object') {
    runenvironment = "arangosh";
  }
  
  _.each(tests, function (file) {
    if (result) {
      print("\n" + Date() + " " + runenvironment + ": Running JSUnity test from file '" + file + "'");
    } 
    else {
      print("\n" + Date() + " " + runenvironment +
            ": Skipping JSUnity test from file '" + file + "' due to previous errors");
    }

    try {
      res = runTest(file, true);
      allResults.push(res);
      result = result && res.status;
      if (! res.status) {
        failed.push(file);
      }
    } 
    catch (err) {
      print(runenvironment + ": cannot run test file '" + file + "': " + err);
      print(err.stack);
      print(err.message);
      result = false;
    }

    internal.wait(0); // force GC
  });
  require("fs").write("testresult.json", JSON.stringify(allResults));

  if (failed.length > 1) {
    print("The following " + failed.length + " test files produced errors: ", failed.join(", "));
  }
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
    isSpecRegEx = /.+-spec.*\.js/,
    isSpec = function (unitTest) {
      return isSpecRegEx.test(unitTest);
    },
    jasmine = _.filter(unitTests, isSpec),
    jsUnity = _.reject(unitTests, isSpec);

  result = runJSUnityTests(jsUnity) && runJasmineTests(jasmine, { format: jasmineReportFormat });

  internal.setUnitTestsResult(result);
}

exports.runCommandLineTests = runCommandLineTests;
