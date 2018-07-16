/* global print */
'use strict';

var runTest = require('jsunity').runTest,
  _ = require('lodash'),
  internal = require('internal');

// //////////////////////////////////////////////////////////////////////////////
  // / @brief runs all jsunity tests
  // //////////////////////////////////////////////////////////////////////////////

function runJSUnityTests (tests) {
  let env = require('internal').env;
  let instanceinfo;
  if (!env.hasOwnProperty('INSTANCEINFO')) {
    throw new Error('env.INSTANCEINFO was not set by caller!');
  }
  instanceinfo = JSON.parse(env.INSTANCEINFO);
  if (!instanceinfo) {
    throw new Error('env.INSTANCEINFO was not set by caller!');
  }
  var result = true;
  var allResults = [];
  var failed = [];
  var res;

  // find out whether we're on server or client...
  var runenvironment = 'arangod';
  if (typeof (require('internal').arango) === 'object') {
    runenvironment = 'arangosh';
  }

  var unitTestFilter = internal.unitTestFilter();
  if (unitTestFilter === "") {
    unitTestFilter = "undefined";
  }

  _.each(tests, function (file) {
    if (result) {
      print('\n' + Date() + ' ' + runenvironment + ": Running JSUnity test from file '" + file + "'");
    } else {
      print('\n' + Date() + ' ' + runenvironment +
        ": Skipping JSUnity test from file '" + file + "' due to previous errors");
    }

    try {
      res = runTest(file, true, unitTestFilter);
      allResults.push(res);
      result = result && res.status;
      if (!res.status) {
        failed.push(file);
      }
    } catch (e) {
      print(runenvironment + ": cannot run test file '" + file + "': " + e);
      let err = e;
      while (err) {
        print(
          err === e
          ? err.stack
          : `via ${err.stack}`
        );
        err = err.cause;
      }
      result = false;
    }

    internal.wait(0); // force GC
  });
  require('fs').write(instanceinfo.rootDir + '/testresult.json', JSON.stringify(allResults));

  if (failed.length > 1) {
    print('The following ' + failed.length + ' test files produced errors: ', failed.join(', '));
  }
  return result;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief runs all mocha tests
// //////////////////////////////////////////////////////////////////////////////

function runMochaTests (testFiles) {
  var result = true;

  if (testFiles.length > 0) {
    print('\nRunning Mocha Tests: ' + testFiles.join(', '));
    result = require('@arangodb/mocha-runner')(testFiles);
  }

  return result;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief runs tests from command-line
// //////////////////////////////////////////////////////////////////////////////

function runCommandLineTests () {
  var result = true,
    unitTests = internal.unitTests(),
    isSpecRegEx = /.+-spec.*\.js/,
    isSpec = function (unitTest) {
      return isSpecRegEx.test(unitTest);
    },
    jsUnity = _.reject(unitTests, isSpec),
    mocha = _.filter(unitTests, isSpec);

  result = (
    runJSUnityTests(jsUnity)
    && runMochaTests(mocha)
  );

  internal.setUnitTestsResult(result);
}

exports.runCommandLineTests = runCommandLineTests;
