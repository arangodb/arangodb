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
  'resilience_move_view': 'resilience "move view" tests',
  'resilience_repair': 'resilience "repair" tests',
  'resilience_failover': 'resilience "failover" tests',
  'resilience_failover_failure': 'resilience "failover failure" tests',
  'resilience_failover_view': 'resilience "failover view" tests',
  'resilience_transactions': 'resilience "transactions" tests',
  'resilience_sharddist': 'resilience "sharddist" tests',
  'resilience_analyzers': 'resilience analyzers tests',
  'client_resilience': 'client resilience tests',
  'active_failover': 'active failover tests'
};
const optionsDocumentation = [
];

const tu = require('@arangodb/testutils/test-utils');
const _ = require('lodash');

const testPaths = {
  'resilience_move': [tu.pathForTesting('server/resilience/move')],
  'resilience_move_view': [tu.pathForTesting('server/resilience/move-view')],
  'resilience_repair': [tu.pathForTesting('server/resilience/repair')],
  'resilience_failover': [tu.pathForTesting('server/resilience/failover')],
  'resilience_failover_failure': [tu.pathForTesting('server/resilience/failover-failure')],
  'resilience_failover_view': [tu.pathForTesting('server/resilience/failover-view')],
  'resilience_transactions': [tu.pathForTesting('server/resilience/transactions')],
  'resilience_sharddist': [tu.pathForTesting('server/resilience/sharddist')],
  'resilience_analyzers': [tu.pathForTesting('server/resilience/analyzers')],
  'client_resilience': [tu.pathForTesting('client/resilience')],
  'active_failover': [tu.pathForTesting('client/active-failover')]
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: resilience*
// //////////////////////////////////////////////////////////////////////////////

var _resilience = function(path) {
  this.func = function resilience (options) {
    let suiteName = path;
    let localOptions = _.clone(options);
    localOptions.cluster = true;
    localOptions.propagateInstanceInfo = true;
    localOptions.oneTestTimeout = 1800;
    if (localOptions.test !== undefined) {
      // remove non ascii characters from our working directory:
      //                                       < A                           > Z && < a                   > z
      suiteName += '_' + localOptions.test.replace(/[\x00-\x40]/g, "_").replace(/[\x5B-\x60]/g, "_").replace(/[\x7B-\xFF]/g, "_");
    }
    if (localOptions.dbServers < 5) {
      localOptions.dbServers = 5;
    }
    let testCases = tu.scanTestPaths(testPaths[path], localOptions);
    let rc = tu.performTests(localOptions, testCases, suiteName, tu.runThere, {
      'javascript.allow-external-process-control': 'true',
      'javascript.allow-port-testing': 'true',
      'javascript.allow-admin-execute': 'true',
    });
    options.cleanup = options.cleanup && localOptions.cleanup;
    return rc;
  };
};

const resilienceMove = (new _resilience('resilience_move')).func;
const resilienceMoveView = (new _resilience('resilience_move_view')).func;
const resilienceRepair = (new _resilience('resilience_repair')).func;
const resilienceFailover = (new _resilience('resilience_failover')).func;
const resilienceFailoverFailure = (new _resilience('resilience_failover_failure')).func;
const resilienceFailoverView = (new _resilience('resilience_failover_view')).func;
const resilienceTransactions = (new _resilience('resilience_transactions')).func;
const resilienceSharddist = (new _resilience('resilience_sharddist')).func;
const resilienceAnalyzers = (new _resilience('resilience_analyzers')).func;

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: client resilience
// //////////////////////////////////////////////////////////////////////////////

function clientResilience (options) {
  let localOptions = _.clone(options);
  localOptions.cluster = true;
  if (localOptions.coordinators < 2) {
    localOptions.coordinators = 2;
  }

  let testCases = tu.scanTestPaths(testPaths.client_resilience, localOptions);
  let rc = tu.performTests(localOptions, testCases, 'client_resilience', tu.runInArangosh, {
    'javascript.allow-external-process-control': 'true',
    'javascript.allow-port-testing': 'true',
    'javascript.allow-admin-execute': 'true',
  });
  options.cleanup = options.cleanup && localOptions.cleanup;
  return rc;
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
  let localOptions = _.clone(options);
  localOptions.activefailover = true;
  localOptions.singles = 4;
  localOptions.disableMonitor = true;
  localOptions.Agency = true;
  let testCases = tu.scanTestPaths(testPaths.active_failover, localOptions);
  let rc = tu.performTests(localOptions, testCases, 'client_resilience', tu.runInArangosh, {
    'server.authentication': 'true',
    'server.jwt-secret': 'haxxmann',
    'javascript.allow-external-process-control': 'true',
    'javascript.allow-port-testing': 'true',
    'javascript.allow-admin-execute': 'true',
  });
  options.cleanup = options.cleanup && localOptions.cleanup;
  return rc;
}

exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['resilience_move'] = resilienceMove;
  testFns['resilience_move_view'] = resilienceMoveView;
  testFns['resilience_repair'] = resilienceRepair;
  testFns['resilience_failover'] = resilienceFailover;
  testFns['resilience_failover_failure'] = resilienceFailoverFailure;
  testFns['resilience_failover_view'] = resilienceFailoverView;
  testFns['resilience_transactions'] = resilienceTransactions;
  testFns['resilience_sharddist'] = resilienceSharddist;
  testFns['resilience_analyzers'] = resilienceAnalyzers;
  testFns['client_resilience'] = clientResilience;
  testFns['active_failover'] = activeFailover;
  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
