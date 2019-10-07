/* global print */
'use strict';

const fs = require('fs');

var runTest = require('jsunity').runTest,
  _ = require('lodash'),
  internal = require('internal');

const deflRes = {
  failed: 0,
  status: true,
  duration: 0,
  total: 0,
  totalSetUp: 0,
  totalTearDown: 0
};
const statusKeys = [
  'failed',
  'status',
  'duration',
  'total',
  'totalSetUp',
  'totalTearDown'
];

// //////////////////////////////////////////////////////////////////////////////
  // / @brief runs all jsunity tests
  // //////////////////////////////////////////////////////////////////////////////

function runJSUnityTests (tests, instanceinfo) {
  let env = internal.env;
  var allResults = _.clone(deflRes);
  var failed = [];
  var res;

  // find out whether we're on server or client...
  var runenvironment = 'arangod';
  if (typeof (internal.arango) === 'object') {
    runenvironment = 'arangosh';
  }

  var unitTestFilter = internal.unitTestFilter();
  if (unitTestFilter === "") {
    unitTestFilter = "undefined";
  }

  _.each(tests, function (file) {
    if (allResults.status) {
      print('\n' + Date() + ' ' + runenvironment + ": Running JSUnity test from file '" + file + "'");
    } else {
      print('\n' + Date() + ' ' + runenvironment +
            ": Skipping JSUnity test from file '" + file + "' due to previous errors");
      if (!allResults.hasOwnProperty('ALLTESTS')) {
        allResults['ALLTESTS'] = {
          status: false,
          skipped: true,
          message: 'test failed, skipping others: '
        };
      }
      allResults.ALLTESTS.message += file + ', ';
      return;
    }

    try {
      res = runTest(file, true, unitTestFilter);
      allResults[file] = res;
      allResults.duration += res.duration;
      allResults.total +=1;
      allResults.failed += res.failed;
      allResults.totalSetUp += res.totalSetup;
      allResults.totalTearDown += res.totalTearDown;

      allResults.status = allResults.status && res.status;

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
      allResults[file] = {
        failed: true,
        message: runenvironment + ": cannot run test file '" + file + "': " + e
      };
      allResults.failed += 1;
      allResults.status = false;
    }

    internal.wait(0); // force GC
  });

  if (failed.length > 1) {
    print('The following ' + failed.length + ' test files produced errors: ', failed.join(', '));
  }
  return allResults;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief runs all mocha tests
// //////////////////////////////////////////////////////////////////////////////

function runMochaTests (testFiles) {
  var result = _.clone(deflRes);

  if (testFiles.length > 0) {
    var unitTestFilter = internal.unitTestFilter();
    if ((unitTestFilter === "") || (unitTestFilter === "undefined")) {
      unitTestFilter = undefined;
    }

    print('\nRunning Mocha Tests: ' + testFiles.join(', '));
    result = require('@arangodb/mocha-runner')(testFiles, true, unitTestFilter);
  }

  return result;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief runs tests from command-line
// //////////////////////////////////////////////////////////////////////////////
function addResults(result, which) {
  for (let key in which) {
    if (which.hasOwnProperty(key)) {
      if (! statusKeys.includes(key)) {
        if (result.hasOwnProperty(key)) {
          print('Duplicate test in "' + key + '" - \n"' + JSON.stringify(which) + "\n" + JSON.stringify(result));
          throw new Error();
        }
        result[key] = which[key];
      }
    }
  }
}

function runCommandLineTests () {
  let instanceinfo;
  let env = internal.env;
  if (!env.hasOwnProperty('INSTANCEINFO')) {
    throw new Error('env.INSTANCEINFO was not set by caller!');
  }
  instanceinfo = JSON.parse(env.INSTANCEINFO);
  if (!instanceinfo) {
    throw new Error('env.INSTANCEINFO was not set by caller!');
  }
  var result,
    unitTests = internal.unitTests(),
    isSpecRegEx = /.+-spec.*\.js/,
    isSpec = function (unitTest) {
      return isSpecRegEx.test(unitTest);
    },
    jsUnity = _.reject(unitTests, isSpec),
    mocha = _.filter(unitTests, isSpec);

  let resultJ = runJSUnityTests(jsUnity, instanceinfo);
  let resultM = runMochaTests(mocha);
  result = {
    failed: resultJ.failed + resultM.failed,
    total: resultJ.total + resultM.total,
    duration: resultJ.duration + resultM.duration,
    status: resultJ.status && resultM.status
  };

  addResults(result, resultJ);
  addResults(result, resultM);
  fs.write(fs.join(instanceinfo.rootDir, 'testresult.json'), JSON.stringify(result));

  internal.setUnitTestsResult(result.status);
}

exports.runCommandLineTests = runCommandLineTests;
