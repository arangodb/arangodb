/* jshint strict: false, sub: true */
/* global */
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
/// @author Manuel Pöter
// //////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'chaos': 'chaos tests',
  'deadlock': 'deadlock tests'
};

const _ = require('lodash');
const tu = require('@arangodb/testutils/test-utils');
const trs = require('@arangodb/testutils/testrunners');

const testPaths = {
  'chaos': [ tu.pathForTesting('client/chaos') ],
  'deadlock': [ tu.pathForTesting('client/chaos') ], // intentionally same path as chaos
};

function chaos (options) {
  let testCasesWithConfigs = {};
  let testCases = tu.scanTestPaths(testPaths.chaos, options).filter((c) => c.includes("test-module-chaos"));
  
  // The chaos test suite is parameterized and each configuration runs 5min.
  // For the nightly tests we want to run a large number of possible parameter
  // combinations, but each file has a runtime limit of 15min and ATM the test
  // system is not designed to allow a test file to be run multiple times with
  // different configurations.
  // The hacky solution here is to build up a map of test cases with their respective
  // configurations and have n copies of the test file in the test case list,
  // where n is the number of configurations for this test. The new `preRun`
  // callback function is called before each testCase execution. At that point we
  // pop the next config from this test case's config list and set the global
  // variable `currentTestConfig` which is then used in the tests.
  // To allow the test cases to define the possible configs I introduced the
  // possibility to write test cases as modules. A jsunity test file that contains
  // "test-module-" in its filename is not executed directly, but instead must
  // export a "run" function that is called. Such a module can optionally define
  // a function "getConfigs" which must return an array of configurations.
  
  testCases = _.flatMap(testCases, testCase => {
    if (testCase.includes("test-module-")) {
      const configProvider = require(testCase).getConfigs;
      if (configProvider) {
        const configs = configProvider();
        if (!Array.isArray(configs)) {
          throw "test case module " + testCase + " does not provide config list";
        }
        testCasesWithConfigs[testCase] = configs;
        return Array(configs.length).fill(testCase);
      }
    }
    return testCase;
  });
 
  testCases = tu.splitBuckets(options, testCases);
  
  class chaosRunner extends trs.runLocalInArangoshRunner {
    preRun(test) {
      global.currentTestConfig = undefined;
      const configs = testCasesWithConfigs[test];
      if (Array.isArray(configs)) {
        if (configs.length === 0) {
          throw "Unexpected! Have no more configs for test case " + test;
        }
        global.currentTestConfig = configs.shift();
      }
    }
    translateResult(testName) {
      let suffix = 'skipped';
      if (global.hasOwnProperty('currentTestConfig') &&
          (global.currentTestConfig !== undefined)) {
        suffix = global.currentTestConfig.suffix;
      }
      return `${testName}_${suffix}`;
    }
  };

  return new chaosRunner(options, 'chaos', {"--server.maximal-threads":"8"}).run(testCases);
}

function deadlock (options) {
  let testCases = tu.scanTestPaths(testPaths.deadlock, options).filter((c) => c.includes("test-deadlock"));
  // start with intentionally few threads, so that deadlocks become more likely
  return new trs.runLocalInArangoshRunner(options, 'deadlock', {"--server.maximal-threads":"8"}).run(testCases);
}

exports.setup = function (testFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['chaos'] = chaos;
  testFns['deadlock'] = deadlock;
  tu.CopyIntoObject(fnDocs, functionsDocumentation);
};
