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
// / @author Max Neunhoeffer
// //////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'gtest': 'gtest test suites'
};
const optionsDocumentation = [
  '   - `skipGTest`: if set to true the gtest unittests are skipped',
  '   - `skipGeo`: obsolete and only here for downwards-compatibility'
];

const fs = require('fs');
const pu = require('@arangodb/process-utils');
const tu = require('@arangodb/test-utils');

const testPaths = {
  'gtest': [],
  'catch': [],
};

const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: GTest
// //////////////////////////////////////////////////////////////////////////////

function locateGTest (name) {
  var file = fs.join(pu.UNITTESTS_DIR, name + pu.executableExt);

  if (!fs.exists(file)) {
    file = fs.join(pu.BIN_DIR, name + pu.executableExt);
    if (!fs.exists(file)) {
      return '';
    }
  }
  return file;
}

function readGreylist() {
  let greylist = [];
  const gtestGreylistRX = new RegExp('- gtest:.*', 'gm');
  let raw_greylist = fs.read(fs.join('tests', 'Greylist.txt'));
  let greylistMatches = raw_greylist.match(gtestGreylistRX);
    if (greylistMatches != null) {
    greylistMatches.forEach(function(match) {
      let partMatch = /- gtest:(.*)/.exec(match);
      if (partMatch.length !== 2) {
        throw new Error("failed to match the test to greylist in: " + match);
      }
      greylist.push(partMatch[1]);
    });
  }
  if (greylist.length !== 0) {
    print(RED + "Greylisting tests: " + JSON.stringify(greylist) + RESET);
  }
  return greylist;
}

function getGTestResults(fileName, defaultResults) {
  let results = defaultResults;
  if (! fs.exists(fileName)) {
    defaultResults.failed += 1;
    print(RED + "No testresult file found at: " + fileName + RESET);    
    return defaultResults;
  }
  let gTestResults = JSON.parse(fs.read(fileName));
  results.failed = gTestResults.failures + gTestResults.errors;
  results.status = (gTestResults.errors === 0) || (gTestResults.failures === 0);
  gTestResults.testsuites.forEach(function(testSuite) {
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

function gtestRunner (options) {
  let results = { failed: 0 };
  let rootDir = fs.join(fs.getTempPath(), 'gtest');
  let testResultJsonFile = fs.join(rootDir, 'testResults.json');
  // we append one cleanup directory for the invoking logic...
  let dummyDir = fs.join(fs.getTempPath(), 'gtest_dummy');
  if (!fs.exists(dummyDir)) {
    fs.makeDirectory(dummyDir);
  }
  pu.cleanupDBDirectoriesAppend(dummyDir);

  const run = locateGTest('arangodbtests');
  if (!options.skipGTest) {
    if (run !== '') {
      let argv = [
        '--log.line-number',
        options.extremeVerbosity ? "true" : "false",
        '--gtest_output=json:' + testResultJsonFile
      ];
      if (options.hasOwnProperty('testCase') && (typeof (options.testCase) !== 'undefined')) {
        argv.push('--gtest_filter='+options.testCase);
      } else {
        argv.push('--gtest_filter=-*_LongRunning');
        let greylist =   readGreylist();
        greylist.forEach(function(greyItem) {
          argv.push('--gtest_filter=-'+greyItem);
        });
      }
      results.basics = pu.executeAndWait(run, argv, options, 'all-gtest', rootDir, options.coreCheck);
      results.basics.failed = results.basics.status ? 0 : 1;
      if (!results.basics.status) {
        results.failed += 1;
      }
      results = getGTestResults(testResultJsonFile, results);
    } else {
      results.failed += 1;
      results.basics = {
        failed: 1,
        status: false,
        message: 'binary "arangodbtests" not found when trying to run suite "all-gtest"'
      };
    }
  }
  return results;
}

exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['gtest'] = gtestRunner;
  testFns['catch'] = gtestRunner;
  testFns['boost'] = gtestRunner;

  opts['skipGtest'] = false;
  opts['skipGeo'] = false; // not used anymore - only here for downwards-compatibility

  defaultFns.push('gtest');

  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
