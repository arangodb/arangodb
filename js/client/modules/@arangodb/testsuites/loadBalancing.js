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
/// @author Dan Larkin-York
/// @author Copyright 2018, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'load_balancing': 'load balancing tests'
};
const optionsDocumentation = [
  '   - `skipLoadBalancing : testing load_balancing will be skipped.'
];

const _ = require('lodash');
const tu = require('@arangodb/testutils/test-utils');

// const BLUE = require('internal').COLORS.COLOR_BLUE;
const CYAN = require('internal').COLORS.COLOR_CYAN;
// const GREEN = require('internal').COLORS.COLOR_GREEN;
const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

const download = require('internal').download;

const testPaths = {
  'load_balancing': [tu.pathForTesting('client/load-balancing')]
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: load_balancing
////////////////////////////////////////////////////////////////////////////////

function loadBalancingClient (options) {
  if (options.skipLoadBalancing === true) {
    print('skipping Load Balancing tests!');
    return {
      load_balancing: {
        status: true,
        skipped: true
      }
    };
  }

  print(CYAN + 'Load Balancing tests...' + RESET);
  const excludeAuth = (fn) => { return (fn.indexOf('-auth') === -1); };
  let testCases = tu.scanTestPaths(testPaths.load_balancing, options)
      .filter(excludeAuth);
  let opts = _.clone(options);
  opts.cluster = true;
  if (opts.coordinators < 2) {
    opts.coordinators = 2;
  }

  let rc = new tu.runInArangoshRunner(opts, 'load_balancing', {
    'server.authentication': 'false'
  }).run(testCases);
  options.cleanup = options.cleanup && opts.cleanup;
  return rc;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: load_balancing_auth
////////////////////////////////////////////////////////////////////////////////

function loadBalancingAuthClient (options) {
  if (options.skipLoadBalancing === true) {
    print('skipping Load Balancing tests!');
    return {
      load_balancing_auth: {
        status: true,
        skipped: true
      }
    };
  }

  print(CYAN + 'Load Balancing with Authentication tests...' + RESET);
  const excludeNoAuth = (fn) => { return (fn.indexOf('-noauth') === -1); };
  let testCases = tu.scanTestPaths(testPaths.load_balancing, options)
                    .filter(excludeNoAuth);
  let opts = _.clone(options);
  opts.cluster = true;
  if (opts.coordinators < 2) {
    opts.coordinators = 2;
  }
  opts.username = 'root';
  opts.password = '';

  let rc = new tu.runInArangoshRunner(opts, 'load_balancing', _.clone(tu.testServerAuthInfo)).run(testCases);
  options.cleanup = options.cleanup && opts.cleanup;
  return rc;
}

exports.setup = function (testFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['load_balancing'] = loadBalancingClient;
  testFns['load_balancing_auth'] = loadBalancingAuthClient;

  opts['skipLoadBalancing'] = false;

  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
