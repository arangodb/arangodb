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
  'resilience': 'resilience tests',
  'client_resilience': 'client resilience tests',
  'cluster_sync': 'cluster sync tests',
  'active_failover': 'active failover tests'
};
const optionsDocumentation = [
];

const tu = require('@arangodb/test-utils');

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: resilience
// //////////////////////////////////////////////////////////////////////////////

function resilience (options) {
  let testCases = tu.scanTestPath('js/server/tests/resilience');
  options.cluster = true;
  options.propagateInstanceInfo = true;
  if (options.dbServers < 5) {
    options.dbServers = 5;
  }
  return tu.performTests(options, testCases, 'resilience', tu.runThere);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: client resilience
// //////////////////////////////////////////////////////////////////////////////

function clientResilience (options) {
  let testCases = tu.scanTestPath('js/client/tests/resilience');
  options.cluster = true;
  if (options.coordinators < 2) {
    options.coordinators = 2;
  }

  return tu.performTests(options, testCases, 'client_resilience', tu.runInArangosh);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: cluster_sync
// //////////////////////////////////////////////////////////////////////////////

function clusterSync (options) {
  if (options.cluster) {
    // may sound strange but these are actually pure logic tests
    // and should not be executed on the cluster
    return {
      'cluster_sync': {
        'status': true,
        'message': 'skipped because of cluster',
        'skipped': true
      }
    };
  }
  let testCases = tu.scanTestPath('js/server/tests/cluster-sync');
  options.propagateInstanceInfo = true;

  return tu.performTests(options, testCases, 'cluster_sync', tu.runThere);
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

  let testCases = tu.scanTestPath('js/client/tests/active-failover');
  options.activefailover = true;
  options.singles = 4;
  return tu.performTests(options, testCases, 'client_resilience', tu.runInArangosh, {
    'server.authentication': 'true',
    'server.jwt-secret': 'haxxmann'
  });
}

function setup (testFns, defaultFns, opts, fnDocs, optionsDoc) {
  testFns['resilience'] = resilience;
  testFns['client_resilience'] = clientResilience;
  testFns['cluster_sync'] = clusterSync;
  testFns['active_failover'] = activeFailover;
  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
}

exports.setup = setup;
