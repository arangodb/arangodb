/* jshint strict: false, sub: true */
/* global print */
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
  'shell_v8': 'Arangodb V8 integration',
  'shell_server_v8': 'Arangodb V8 integration run inside of coordinator / dbserver',
  'shell_api': 'shell client tests - only *api*',
  'shell_api_multi': 'shell client tests - only *api* - to be run in multi protocol environments',
  'shell_client': 'shell client tests',
  'shell_client_multi': 'shell client tests to be run in multiple protocol environments',
  'shell_server': 'shell server tests',
  'shell_client_aql': 'AQL tests in the client',
  'shell_server_aql': 'AQL tests in the server',
  'shell_server_only': 'server specific tests',
  'shell_client_transaction': 'transaction tests',
  'shell_client_replication2_recovery': 'replication2 cluster recovery tests',
  'shell_client_traffic': 'traffic metrics tests',
};
const optionsDocumentation = [
  '   - `skipAql`: if set to true the AQL tests are skipped',
  '   - `skipGraph`: if set to true the graph tests are skipped'
];

const _ = require('lodash');
const tu = require('@arangodb/testutils/test-utils');
const fs = require('fs');
const GREEN = require('internal').COLORS.COLOR_GREEN;
const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
const YELLOW = require('internal').COLORS.COLOR_YELLOW;

const testPaths = {
  'shell_v8': [ tu.pathForTesting('common/v8')],
  'shell_server_v8': [ tu.pathForTesting('common/v8')],
  'shell_api': [ tu.pathForTesting('client/shell/api')],
  'shell_api_multi': [ tu.pathForTesting('client/shell/api/multi')],
  'shell_client': [ tu.pathForTesting('common/shell'), tu.pathForTesting('client/shell')],
  'shell_client_multi': [ tu.pathForTesting('common/shell/multi'), tu.pathForTesting('client/shell/multi')],
  'shell_server': [ tu.pathForTesting('common/shell'), tu.pathForTesting('server/shell') ],
  'shell_server_only': [ tu.pathForTesting('server/shell') ],
  'shell_server_aql': [ tu.pathForTesting('server/aql'), tu.pathForTesting('common/aql') ],
  'shell_client_aql': [ tu.pathForTesting('client/aql'), tu.pathForTesting('common/aql') ],
  'shell_client_transaction': [ tu.pathForTesting('client/shell/transaction')],
  'shell_client_replication2_recovery': [ tu.pathForTesting('client/shell/transaction/replication2_recovery')],
  'shell_client_traffic': [ tu.pathForTesting('client/shell/traffic') ],
};

/// ensure that we have enough db servers in cluster tests
function ensureServers(options, numServers) {
  if (options.cluster && options.dbServers < numServers) {
    let localOptions = _.clone(options);
    localOptions.dbServers = numServers;
    return localOptions;
  }
  return options;
}

/// ensure that we have enough coordinators in cluster tests
function ensureCoordinators(options, numServers) {
  if (options.cluster && options.coordinators < numServers) {
    let localOptions = _.clone(options);
    localOptions.coordinators = numServers;
    return localOptions;
  }
  return options;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: shell_v8
// //////////////////////////////////////////////////////////////////////////////

class shellv8Runner extends tu.runLocalInArangoshRunner {
  constructor(options, testname, ...optionalArgs) {
    super(options, testname, ...optionalArgs);
    this.info = "shellv8Runner";
  }

  run(testcases) {
    let obj = this;
    let res = {failed: 0, status: true};
    let filtered = {};
    let rootDir = fs.join(fs.getTempPath(), 'shellv8Runner');
    this.instanceManager = {
      rootDir: rootDir,
      endpoint: 'tcp://127.0.0.1:8888',
      findEndpoint: function() {
        return 'tcp://127.0.0.1:8888';
      },
      getStructure: function() {
        return {
          endpoint: 'tcp://127.0.0.1:8888',
          rootDir: rootDir
        };
      }
    };
    let count = 0;
    fs.makeDirectoryRecursive(rootDir);
    testcases.forEach(function (file, i) {
      if (tu.filterTestcaseByOptions(file, obj.options, filtered)) {
        print('\n' + (new Date()).toISOString() + GREEN + " [============] RunInV8: Trying", file, '... ' + count, RESET);
        res[file] = obj.runOneTest(file);
        if (res[file].status === false) {
          res.failed += 1;
          res.status = false;
        }
      } else if (obj.options.extremeVerbosity) {
        print('Skipped ' + file + ' because of ' + filtered.filter);
      }
      count += 1;
    });
    if (count === 0) {
      res['ALLTESTS'] = {
        status: true,
        skipped: true
      };
      res.status = true;
      print(RED + 'No testcase matched the filter.' + RESET);
    }
    return res;
  }
}

function shellV8 (options) {
  let testCases = tu.scanTestPaths(testPaths.shell_v8, options);
  testCases = tu.splitBuckets(options, testCases);
  let rc = new shellv8Runner(options, 'shell_v8', []).run(testCases);
  return rc;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: shell_server_v8
// //////////////////////////////////////////////////////////////////////////////

function shellV8Server (options) {
  let testCases = tu.scanTestPaths(testPaths.shell_server_v8, options);
  testCases = tu.splitBuckets(options, testCases);
  let rc = new tu.runOnArangodRunner(options, 'shell_v8', []).run(testCases);
  return rc;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: shell_api
// //////////////////////////////////////////////////////////////////////////////

function shellApiClient (options) {
  let testCases = tu.scanTestPaths(testPaths.shell_api, options);

  testCases = tu.splitBuckets(options, testCases);

  var opts = ensureServers(options, 3);
  opts = ensureCoordinators(opts, 2);
  opts['httpTrustedOrigin'] =  'http://was-erlauben-strunz.it';

  // increase timeouts after which servers count as BAD/FAILED.
  // we want this to ensure that in an overload situation we do not
  // get random failedLeader / failedFollower jobs during our tests.
  let moreOptions = { "agency.supervision-ok-threshold" : "15", "agency.supervision-grace-period" : "30" };
  let rc = new tu.runLocalInArangoshRunner(opts, 'shell_api', moreOptions).run(testCases);
  options.cleanup = options.cleanup && opts.cleanup;
  return rc;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: shell_api_multi
// //////////////////////////////////////////////////////////////////////////////

function shellApiMulti (options) {
  let testCases = tu.scanTestPaths(testPaths.shell_api_multi, options);

  testCases = tu.splitBuckets(options, testCases);

  var opts = ensureServers(options, 3);
  opts = ensureCoordinators(opts, 2);
  opts['httpTrustedOrigin'] =  'http://was-erlauben-strunz.it';

  // increase timeouts after which servers count as BAD/FAILED.
  // we want this to ensure that in an overload situation we do not
  // get random failedLeader / failedFollower jobs during our tests.
  let moreOptions = { "agency.supervision-ok-threshold" : "15", "agency.supervision-grace-period" : "30" };
  let rc = new tu.runLocalInArangoshRunner(opts, 'shell_api_multi', moreOptions).run(testCases);
  options.cleanup = options.cleanup && opts.cleanup;
  return rc;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: shell_client
// //////////////////////////////////////////////////////////////////////////////

function shellClient (options) {
  let testCases = tu.scanTestPaths(testPaths.shell_client, options);

  testCases = tu.splitBuckets(options, testCases);

  var opts = ensureServers(options, 3);
  opts = ensureCoordinators(opts, 2);
  opts['httpTrustedOrigin'] =  'http://was-erlauben-strunz.it';

  // increase timeouts after which servers count as BAD/FAILED.
  // we want this to ensure that in an overload situation we do not
  // get random failedLeader / failedFollower jobs during our tests.
  let moreOptions = { "agency.supervision-ok-threshold" : "15", "agency.supervision-grace-period" : "30" };
  let rc = new tu.runLocalInArangoshRunner(opts, 'shell_client', moreOptions).run(testCases);
  options.cleanup = options.cleanup && opts.cleanup;
  return rc;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: shell_client_multi
// //////////////////////////////////////////////////////////////////////////////

function shellClientMulti (options) {
  let testCases = tu.scanTestPaths(testPaths.shell_client_multi, options);

  testCases = tu.splitBuckets(options, testCases);

  var opts = ensureServers(options, 3);
  opts = ensureCoordinators(opts, 2);
  opts['httpTrustedOrigin'] =  'http://was-erlauben-strunz.it';

  // increase timeouts after which servers count as BAD/FAILED.
  // we want this to ensure that in an overload situation we do not
  // get random failedLeader / failedFollower jobs during our tests.
  let moreOptions = { "agency.supervision-ok-threshold" : "15", "agency.supervision-grace-period" : "30" };
  let rc = new tu.runLocalInArangoshRunner(opts, 'shell_client_multi', moreOptions).run(testCases);
  options.cleanup = options.cleanup && opts.cleanup;
  return rc;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: shell_server
// //////////////////////////////////////////////////////////////////////////////

function shellServer (options) {
  options.propagateInstanceInfo = true;

  let testCases = tu.scanTestPaths(testPaths.shell_server, options);

  testCases = tu.splitBuckets(options, testCases);

  let opts = ensureServers(options, 3);
  let rc = new tu.runOnArangodRunner(opts, 'shell_server', {}).run(testCases);
  options.cleanup = options.cleanup && opts.cleanup;
  return rc;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: shell_server_only
// //////////////////////////////////////////////////////////////////////////////

function shellServerOnly (options) {
  let testCases = tu.scanTestPaths(testPaths.shell_server_only, options);

  testCases = tu.splitBuckets(options, testCases);

  let opts = ensureServers(options, 3);
  let rc = new tu.runOnArangodRunner(opts, 'shell_server_only', {}).run(testCases);
  options.cleanup = options.cleanup && opts.cleanup;
  return rc;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: shell_server_aql
// //////////////////////////////////////////////////////////////////////////////

function shellServerAql (options) {
  let testCases;
  let name = 'shell_server_aql';

  if (!options.skipAql) {
    testCases = tu.scanTestPaths(testPaths.shell_server_aql, options);
    if (options.skipRanges) {
      testCases = _.filter(testCases,
                           function (p) { return p.indexOf('ranges-combined') === -1; });
      name = 'shell_server_aql_skipranges';
    }

    testCases = tu.splitBuckets(options, testCases);

    let opts = ensureServers(options, 3);
    let rc = new tu.runOnArangodRunner(opts, name, {}).run(testCases);
    options.cleanup = options.cleanup && opts.cleanup;
    return rc;
  }

  return {
    shell_server_aql: {
      status: true,
      skipped: true
    }
  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: shell_client_aql
// //////////////////////////////////////////////////////////////////////////////

function shellClientAql (options) {
  let testCases;
  let name = 'shell_client_aql';

  if (!options.skipAql) {
    testCases = tu.scanTestPaths(testPaths.shell_client_aql, options);
    if (options.skipRanges) {
      testCases = _.filter(testCases,
                           function (p) { return p.indexOf('ranges-combined') === -1; });
      name = 'shell_client_aql_skipranges';
    }

    testCases = tu.splitBuckets(options, testCases);

    let opts = ensureServers(options, 3);
    let rc = new tu.runLocalInArangoshRunner(opts, name, {}).run(testCases);
    options.cleanup = options.cleanup && opts.cleanup;
    return rc;
  }

  return {
    shell_client_aql: {
      status: true,
      skipped: true
    }
  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: shell_client_traffic
// //////////////////////////////////////////////////////////////////////////////

function shellClientTraffic(options) {
  let testCases = tu.scanTestPaths(testPaths.shell_client_traffic, options);
  testCases = tu.splitBuckets(options, testCases);

  let opts = ensureServers(options, 3);
  opts['httpTrustedOrigin'] =  'http://was-erlauben-strunz.it';

  let rc = new tu.runLocalInArangoshRunner(opts, 'shell_client_traffic', {}).run(testCases);
  options.cleanup = options.cleanup && opts.cleanup;
  return rc;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: shell_client_transaction
// //////////////////////////////////////////////////////////////////////////////

function shellClientTransaction(options) {
  let testCases = tu.scanTestPaths(testPaths.shell_client_transaction, options);
  testCases = tu.splitBuckets(options, testCases);

  let opts = ensureServers(options, 3);
  opts['httpTrustedOrigin'] =  'http://was-erlauben-strunz.it';

  let moreOptions = {
    "agency.supervision-ok-threshold": "1.5",
    "agency.supervision-grace-period": "3.0",
  };
  let rc = new tu.runLocalInArangoshRunner(opts, 'shell_client_transaction', moreOptions).run(testCases);
  options.cleanup = options.cleanup && opts.cleanup;
  return rc;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: shell_client_replication2_recovery
// //////////////////////////////////////////////////////////////////////////////

function shellClientReplication2Recovery(options) {
  let testCases = tu.scanTestPaths(testPaths.shell_client_replication2_recovery, options);
  testCases = tu.splitBuckets(options, testCases);

  var opts = ensureServers(options, 5);
  opts = ensureCoordinators(opts, 2);
  opts.enableAliveMonitor = false;
  opts['httpTrustedOrigin'] =  'http://was-erlauben-strunz.it';

  let moreOptions = {
    'javascript.allow-external-process-control': 'true',
    'javascript.allow-admin-execute': 'true',
    'javascript.allow-port-testing': 'true',
    "agency.supervision-ok-threshold": "1.5",
    "agency.supervision-grace-period": "3.0",
  };
  let rc = new tu.runLocalInArangoshRunner(opts, 'shell_client_replication2_recovery', moreOptions).run(testCases);
  options.cleanup = options.cleanup && opts.cleanup;
  return rc;
}

exports.setup = function (testFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['shell_v8'] = shellV8;
  testFns['shell_server_v8'] = shellV8Server;
  testFns['shell_api'] = shellApiClient;
  testFns['shell_api_multi'] = shellApiMulti;
  testFns['shell_client'] = shellClient;
  testFns['shell_client_multi'] = shellClientMulti;
  testFns['shell_server'] = shellServer;
  testFns['shell_client_aql'] = shellClientAql;
  testFns['shell_server_aql'] = shellServerAql;
  testFns['shell_server_only'] = shellServerOnly;
  testFns['shell_client_transaction'] = shellClientTransaction;
  testFns['shell_client_replication2_recovery'] = shellClientReplication2Recovery;
  testFns['shell_client_traffic'] = shellClientTraffic;

  opts['skipAql'] = false;
  opts['skipRanges'] = true;

  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
