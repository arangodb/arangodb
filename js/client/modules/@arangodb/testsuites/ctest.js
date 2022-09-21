/* jshint strict: false, sub: true */
/* global print */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2016 ArangoDB GmbH, Cologne, Germany
// / Copyright 2014 triagens GmbH, Cologne, Germany
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
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Markus Pfeiffer
// //////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'ctest': 'run ctest test suites'
};

const fs = require('fs');
const pu = require('@arangodb/testutils/process-utils');
const tu = require('@arangodb/testutils/test-utils');
const tmpDirMmgr = require('@arangodb/testutils/tmpDirManager').tmpDirManager;
const testPaths = {
  'gtest': [],
  'catch': [],
};

const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;

function getCTestResults(fileName, defaultResults) {
  let results = defaultResults;
  if (! fs.exists(fileName)) {
    defaultResults.failed += 1;
    print(RED + "No testresult file found at: " + fileName + RESET);    
    return defaultResults;
  }
  let cTestResults = JSON.parse(fs.read(fileName));
  results.failed = cTestResults.failures + gTestResults.errors;
  results.status = (cTestResults.errors === 0) || (cTestResults.failures === 0);
  cTestResults.testsuites.forEach(function(testSuite) {
    results[testSuite.name] = {
      failed: testSuite.failures + testSuite.errors,
      status: (testSuite.failures + testSuite.errors ) === 0,
      duration: testSuite.time
    };
    if (testSuite.failures !== 0) {
      let message = "";
      testSuite.testsuite.forEach(function (suite) {
        if (suite.hasOwnProperty('failures')) {
          suite.failures.forEach(function (fail) {
            message += fail.failure;
          });
        }
      });
      results[testSuite.name].message = message;
    }
  });
  return results;
}

function ctestRunner (options) {
  let results = { failed: 0 };
  let rootDir = fs.join(fs.getTempPath(), 'ctest');
  let testResultJUnitFile = fs.join(rootDir, 'testResults.xml');

  if (!options.skipGTest) {
    let tmpMgr = new tmpDirMmgr('gtest', options);
    let argv = [
      '--test-dir', pu.BUILD_DIR,
      '--output-log', testResultJUnitFile,
    ];
    if (options.hasOwnProperty('testCase') && (typeof (options.testCase) !== 'undefined')) {
      argv.push('--tests-regex'); argv.push(options.testCase);
    } else {
      argv.push('--exclude-regex'); argv.push('.*Death.*');
    }
    argv.push(options.extremeVerbosity ? "true" : "false");
    argv.push(options.disableMonitor, true);
    results.basics = pu.executeAndWait("ctest", argv, options, 'all-ctest', rootDir, options.coreCheck);
    results.basics.failed = results.basics.status ? 0 : 1;
    if (!results.basics.status) {
      results.failed += 1;
    }
    results = getCTestResults(testResultJUnitFile, results);
    tmpMgr.destructor((results.failed === 0) && options.cleanup);
  }
  return results;
}

exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['ctest'] = ctestRunner;

  defaultFns.push('ctest');

  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
};
