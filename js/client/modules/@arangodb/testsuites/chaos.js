/* jshint strict: false, sub: true */
/* global */
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License")
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'chaos': 'chaos tests'
};
const optionsDocumentation = [];

const _ = require('lodash');
const tu = require('@arangodb/testutils/test-utils');

const testPaths = {
  'chaos': [ tu.pathForTesting('client/chaos') ],
};

function chaos (options) {
  let testCasesWithConfigs = {};
  let testCases = tu.scanTestPaths(testPaths.chaos, options);
  
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
  
  let handlers = {
    preRun: (test) => {
      global.currentTestConfig = undefined;
      const configs = testCasesWithConfigs[test];
      if (Array.isArray(configs)) {
        if (configs.length === 0) {
          throw "Unexpected! Have no more configs for test case " + test;
        }
        global.currentTestConfig = configs.shift();
      }
    }
  };

  return tu.performTests(options, testCases, 'chaos', tu.runInLocalArangosh, {}, handlers);
}

exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['chaos'] = chaos;
  
  // intentionally not turned on by default, as the suite may take a lot of time
  // defaultFns.push('chaos');

  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
