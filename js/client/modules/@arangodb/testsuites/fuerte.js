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
  'fuerte': 'fuerte gtest test suites'
};

const fs = require('fs');
const pu = require('@arangodb/testutils/process-utils');
const tu = require('@arangodb/testutils/test-utils');
const im = require('@arangodb/testutils/instance-manager');
const {getGTestResults} = require('@arangodb/testutils/result-processing');

const testPaths = {
  'fuerte': []
};

const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: GTest
// //////////////////////////////////////////////////////////////////////////////

function locateGTest(name) {
  var file = fs.join(pu.UNITTESTS_DIR, name + pu.executableExt);

  if (!fs.exists(file)) {
    file = fs.join(pu.BIN_DIR, name + pu.executableExt);
    if (!fs.exists(file)) {
      return '';
    }
  }
  return file;
}

function gtestRunner(options) {
  let results = { failed: 0 };
  let rootDir = fs.join(fs.getTempPath(), 'fuertetest');
  let testResultJsonFile = fs.join(rootDir, 'testResults.json');

  const run = locateGTest('fuertetest');

  if (run === '') {
    results.failed += 1;
    results.fuerte = {
      failed: 1,
      status: false,
      message: 'binary "fuertetest" not found when trying to run suite "fuertetest"'
    };
    return results;
  }

  // start server
  print('Starting ' + options.protocol + ' server...');

  let instanceManager = new im.instanceManager(options.protocol, options, {
    "http.keep-alive-timeout": "10",
  }, 'fuerte');
  instanceManager.prepareInstance();
  instanceManager.launchTcpDump("");
  if (!instanceManager.launchInstance()) {
    return {
      fuerte: {
        status: false,
        message: 'failed to start server!'
      }
    };
  }
  instanceManager.reconnect();

  let argv = [
    '--gtest_output=json:' + testResultJsonFile
  ];
  if (options.hasOwnProperty('testCase') && (typeof (options.testCase) !== 'undefined')) {
    argv.push('--gtest_filter=' + options.testCase);
  }
  // all non gtest args have to come last
  argv.push('--log.line-number');
  argv.push(options.extremeVerbosity ? "true" : "false");

  // TODO use JWT tokens ?
  argv.push('--endpoint=' + instanceManager.endpoint);
  argv.push('--authentication=' + "basic:root:");

  print(argv);

  results.fuerte = pu.executeAndWait(run, argv, options, 'fuertetest', rootDir, options.coreCheck);
  results.fuerte.failed = results.fuerte.status ? 0 : 1;
  if (!results.fuerte.status) {
    results.failed += 1;
  }
  results = getGTestResults(testResultJsonFile, results);

  print('Shutting down...');

  results['shutdown'] = instanceManager.shutdownInstance(false);
  instanceManager.destructor(results.status);

  return results;
}

exports.setup = function (testFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['fuerte'] = gtestRunner;

  tu.CopyIntoObject(fnDocs, functionsDocumentation);
};
