/* jshint strict: false, sub: true */
/* global */
'use strict';

// /////////////////////////////////////////////////////////////////////////////
// DISCLAIMER
// 
// Copyright 2016-2018 ArangoDB GmbH, Cologne, Germany
// Copyright 2014 triagens GmbH, Cologne, Germany
// 
// Licensed under the Apache License, Version 2.0 (the "License")
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//     http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// 
// Copyright holder is ArangoDB GmbH, Cologne, Germany
// 
// @author Max Neunhoeffer
// /////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'resilience': 'resilience tests',
  'client_resilience': 'client resilience tests',
  'active_failover': 'active failover tests'
};
const optionsDocumentation = [
];

const tu = require('@arangodb/test-utils');

const testPaths = {
  'resilience': [tu.pathForTesting('server/resilience')],
  'client_resilience': [tu.pathForTesting('client/resilience')],
  'active_failover': [tu.pathForTesting('client/active-failover')]
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: resilience
// //////////////////////////////////////////////////////////////////////////////

function resilience (options) {
  let suiteName = 'resilience';
  let testCases = tu.scanTestPaths(testPaths.resilience);
  options.cluster = true;
  options.propagateInstanceInfo = true;
  if (options.test !== undefined) {
    // remove non ascii characters from our working directory:
    //                                       < A                           > Z && < a                   > z
    suiteName += '_' + options.test.replace(/[\x00-\x40]/g, "_").replace(/[\x5B-\x60]/g, "_").replace(/[\x7B-\xFF]/g, "_");
  }
  if (options.dbServers < 5) {
    options.dbServers = 5;
  }
  return tu.performTests(options, testCases, suiteName, tu.runThere);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: client resilience
// //////////////////////////////////////////////////////////////////////////////

function clientResilience (options) {
  let testCases = tu.scanTestPaths(testPaths.client_resilience);
  options.cluster = true;
  if (options.coordinators < 2) {
    options.coordinators = 2;
  }

  return tu.performTests(options, testCases, 'client_resilience', tu.runInArangosh);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: active failover
// //////////////////////////////////////////////////////////////////////////////

function activeFailover (options) {
  if (options.cluster) {
    return {
      'active_failover': {
        'status': true,
        'message': 'skipped because of cluster',
        'skipped': true
      }
    };
  }

  let testCases = tu.scanTestPaths(testPaths.active_failover);
  options.activefailover = true;
  options.singles = 4;
  return tu.performTests(options, testCases, 'client_resilience', tu.runInArangosh, {
    'server.authentication': 'true',
    'server.jwt-secret': 'haxxmann'
  });
}

exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['resilience'] = resilience;
  testFns['client_resilience'] = clientResilience;
  testFns['active_failover'] = activeFailover;
  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
