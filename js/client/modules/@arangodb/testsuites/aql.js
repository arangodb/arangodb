/* jshint strict: false, sub: true */
/* global */
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
  'shell_client': 'shell client tests',
  'shell_server': 'shell server tests',
  'shell_server_aql': 'AQL tests in the server',
  'shell_server_only': 'server specific tests'
};
const optionsDocumentation = [
  '   - `skipAql`: if set to true the AQL tests are skipped',
  '   - `skipGraph`: if set to true the graph tests are skipped'
];

const _ = require('lodash');
const tu = require('@arangodb/test-utils');

const testPaths = {
  'shell_client': [ 'js/common/tests/shell', 'js/client/tests/http', 'js/client/tests/shell' ],
  'shell_server': [ 'js/common/tests/shell', 'js/server/tests/shell' ],
  'shell_server_only': [ 'js/server/tests/shell' ],
  'shell_server_aql': [ 'js/server/tests/aql' ]
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: shell_client
// //////////////////////////////////////////////////////////////////////////////

function shellClient (options) {
  let testCases = tu.scanTestPath(testPaths.shell_client[0]);
  testCases = testCases.concat(tu.scanTestPath(testPaths.shell_client[1]));
  testCases = testCases.concat(tu.scanTestPath(testPaths.shell_client[2]));

  return tu.performTests(options, testCases, 'shell_client', tu.runInArangosh);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: shell_server
// //////////////////////////////////////////////////////////////////////////////

function shellServer (options) {
  options.propagateInstanceInfo = true;

  let testCases = tu.scanTestPath(testPaths.shell_server[0]);
  testCases = testCases.concat(tu.scanTestPath(testPaths.shell_server[1]));

  return tu.performTests(options, testCases, 'shell_server', tu.runThere);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: shell_server_only
// //////////////////////////////////////////////////////////////////////////////

function shellServerOnly (options) {
  let testCases = tu.scanTestPath(testPaths.shell_server_only[0]);

  return tu.performTests(options, testCases, 'shell_server_only', tu.runThere);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: shell_server_aql
// //////////////////////////////////////////////////////////////////////////////

function shellServerAql (options) {
  let testCases;
  let name = 'shell_server_aql';

  if (!options.skipAql) {
    testCases = tu.scanTestPath(testPaths.shell_server_aql[0]);
    if (options.skipRanges) {
      testCases = _.filter(testCases,
                           function (p) { return p.indexOf('ranges-combined') === -1; });
      name = 'shell_server_aql_skipranges';
    }

    testCases = tu.splitBuckets(options, testCases);

    return tu.performTests(options, testCases, name, tu.runThere);
  }

  return {
    shell_server_aql: {
      status: true,
      skipped: true
    }
  };
}

exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['shell_client'] = shellClient;
  testFns['shell_server'] = shellServer;
  testFns['shell_server_aql'] = shellServerAql;
  testFns['shell_server_only'] = shellServerOnly;

  defaultFns.push('shell_client');
  defaultFns.push('shell_server');
  defaultFns.push('shell_server_aql');

  opts['skipAql'] = false;
  opts['skipRanges'] = true;

  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
