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
  'resilience_move': 'resilience "move" tests',
  'resilience_repair': 'resilience "repair" tests',
  'resilience_failover': 'resilience "failover" tests',
  'resilience_failover_failure': 'resilience "failover failure" tests',
  'resilience_sharddist': 'resilience "sharddist" tests',
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

var _resilience = function(path) {
  this.func = function resilience (options) {
    let testCases = tu.scanTestPath(path);
    options.cluster = true;
    options.propagateInstanceInfo = true;
    if (options.dbServers < 5) {
      options.dbServers = 5;
    }
    return tu.performTests(options, testCases, path, tu.runThere);
  };
};
const resilienceMove = (new _resilience('js/server/tests/resilience/move')).func;
const resilienceRepair = (new _resilience('js/server/tests/resilience/repair/')).func;
const resilienceFailover = (new _resilience('js/server/tests/resilience/failover')).func;
const resilienceFailoverFailure = (new _resilience('js/server/tests/resilience/failover-failure')).func;
const resilienceSharddist = (new _resilience('js/server/tests/resilience/sharddist')).func;

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
  testFns['resilience_move'] = resilienceMove;
  testFns['resilience_repair'] = resilienceRepair;
  testFns['resilience_failover'] = resilienceFailoverFailure;
  testFns['resilience_failover_failure'] = resilienceFailoverFailure;
  testFns['resilience_sharddist'] = resilienceSharddist;
  testFns['client_resilience'] = clientResilience;
  testFns['cluster_sync'] = clusterSync;
  testFns['active_failover'] = activeFailover;
  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
}

exports.setup = setup;
