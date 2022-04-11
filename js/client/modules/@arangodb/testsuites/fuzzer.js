/* jshint strict: false, sub: true */
/* global */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2022 ArangoDB GmbH, Cologne, Germany
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
// / @author Julia Puget
// //////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'shell_fuzzer': 'shell client fuzzer tests'
};

const _ = require('lodash');
const tu = require('@arangodb/testutils/test-utils');

const testPaths = {
  'shell_fuzzer': [tu.pathForTesting('client/fuzz')]
};


// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: shell_fuzzer
// //////////////////////////////////////////////////////////////////////////////

function shellFuzzer(options) {
  if (!global.ARANGODB_CLIENT_VERSION(true)['failure-tests'] ||
      global.ARANGODB_CLIENT_VERSION(true)['failure-tests'] === 'false') {
    return {
      recovery: {
        status: false,
        message: 'failure-tests not enabled. please recompile with -DUSE_FAILURE_TESTS=On'
      },
      status: false
    };
  }

  let testCases = tu.scanTestPaths(testPaths.shell_fuzzer, options);

  testCases = tu.splitBuckets(options, testCases);
  let rc = new tu.runLocalInArangoshRunner(options, 'shell_fuzzer').run(testCases);
  return rc;
}

exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['shell_fuzzer'] = shellFuzzer;

  defaultFns.push('shell_fuzzer');

  for (var attrname in functionsDocumentation) {
    fnDocs[attrname] = functionsDocumentation[attrname];
  }
};
