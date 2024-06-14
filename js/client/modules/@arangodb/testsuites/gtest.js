/* jshint strict: false, sub: true */
/* global print */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

const _ = require('lodash');
const fs = require('fs');
const pu = require('@arangodb/testutils/process-utils');
const tu = require('@arangodb/testutils/test-utils');
const tmpDirMmgr = require('@arangodb/testutils/tmpDirManager').tmpDirManager;
const {getGTestResults} = require('@arangodb/testutils/result-processing');
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

function gtestRunner (testfilename, name, opts, testoptions) {
  let options = _.clone(opts);
  if (testoptions !== undefined) {
    if (!options.commandSwitches) {
      options.commandSwitches = [];
    }
    options.commandSwitches = options.commandSwitches.concat(testoptions);
  }
  let results = { failed: 0 };
  let rootDir = fs.join(fs.getTempPath(), name);
  let testResultJsonFile = fs.join(rootDir, 'testResults.json');

  const binary = locateGTest(testfilename);
  if (binary !== '') {
    let tmpMgr = new tmpDirMmgr('gtest', options);
    let argv = [
      '--gtest_output=json:' + testResultJsonFile,
    ];

    let filter = options.commandSwitches ? options.commandSwitches.filter(s => s.startsWith("gtest_filter=")) : [];
    if (filter.length > 0) {
      if (filter.length > 1) {
        throw "Found more than one gtest_filter argument";
      }
      argv.push('--' + filter[0]);
    } else if (options.hasOwnProperty('testCase') && (typeof (options.testCase) !== 'undefined')) {
      argv.push('--gtest_filter='+options.testCase);
    } else {
      argv.push('--gtest_filter=-*_LongRunning');
    }
    // all non gtest args have to come last
    argv.push('--log.line-number');
    argv.push(options.extremeVerbosity ? "true" : "false");
    results[name] = pu.executeAndWait(binary, argv, options, 'all-gtest', rootDir, options.coreCheck);
    results[name].failed = results[name].status ? 0 : 1;
    if (!results[name].status) {
      results.failed += 1;
    }
    results = getGTestResults(testResultJsonFile, results);
    tmpMgr.destructor((results.failed === 0) && options.cleanup);
  } else {
    results.failed += 1;
    results[name] = {
      failed: 1,
      status: false,
      message: `binary ${testfilename} not found when trying to run suite "all-gtest"`
    };
  }
  return results;
}

exports.setup = function (testFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);

  const tests = [ 'arangodbtests_zkd' ];

  for(const test of tests) {
    testFns[test] = x => gtestRunner(test, test, x);
  }
  testFns['gtest'] = x => gtestRunner('arangodbtests', 'arangodbtests', x);
  
  let iresearch_filter = ['gtest_filter=IResearch*'];
  testFns['gtest_iresearch'] = x => gtestRunner('arangodbtests', 'gtest-iresearch', x, iresearch_filter);
  let no_iresearch_filter = ['gtest_filter=-IResearch*:*_LongRunning*'];
  testFns['gtest_arangodb'] = x => gtestRunner('arangodbtests', 'gtest-arangodb', x, no_iresearch_filter);

  tu.CopyIntoObject(fnDocs, functionsDocumentation);
};
