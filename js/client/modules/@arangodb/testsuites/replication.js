/* jshint strict: false, sub: true */
/* global print, arango */
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
// //////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'replication_static': 'replication static tests',
  'replication_sync': 'replication sync tests',
  'shell_replication': 'shell replication tests',
  'http_replication': 'client replication API tests'
};
const optionsDocumentation = [
];

const _ = require('lodash');
const pu = require('@arangodb/testutils/process-utils');
const tu = require('@arangodb/testutils/test-utils');
const trs = require('@arangodb/testutils/testrunners');
const ct = require('@arangodb/testutils/client-tools');

const testPaths = {
  'shell_replication': [tu.pathForTesting('common/replication')],
  'replication_static': [tu.pathForTesting('client/replication/static')],
  'replication_sync': [tu.pathForTesting('client/replication/sync')],
  'http_replication': [tu.pathForTesting('common/replication_api')]
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: shell_replication
// //////////////////////////////////////////////////////////////////////////////

function shellReplication (options) {
  let testCases = tu.scanTestPaths(testPaths.shell_replication, options);

  var opts = {
    'replication': true,
    'jwtSecret': 'helloreplication'
  };
  _.defaults(opts, options);

  return new trs.runLocalInArangoshRunner(opts, 'shell_replication').run(testCases);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: shell_replication
// //////////////////////////////////////////////////////////////////////////////

function shellClientReplicationApi (options) {
  let testCases = tu.scanTestPaths(testPaths.http_replication, options);

  var opts = {
    'replication': true,
  };
  
  arango.forceJson(true);
  _.defaults(opts, options);
  opts.forceJson = true;

  let ret = new trs.runLocalInArangoshRunner(opts, 'shell_replication_api').run(testCases);
  if (!options.forceJson) {
    arango.forceJson(false);
  }
  return ret;
}


class replicationRunner extends trs.runLocalInArangoshRunner {
  constructor(options, testname, serverOptions, startReplication=false) {
    super(options, testname, serverOptions);
    this.options.singles = 2;
    this.follower = undefined;
    this.addArgs = {};
    this.startReplication = startReplication;
  }

  preStart() {
    // our tests lean on accessing the `_users` collection, hence no auth for secondary
    this.instanceManager.arangods[1].args['server.authentication'] = false;
    return {
      message: '',
      state: true,
    };
  }
  postStart() {
    let message;
    print("starting replication follower: ");
    let state = true;
    this.addArgs['flatCommands'] = [this.instanceManager.arangods[1].endpoint];
    if (this.startReplication) {
      [0, 1].forEach(which => {
        this.instanceManager.endpoint = this.instanceManager.arangods[which].endpoint;
        this.instanceManager.arangods[which].connect();
        let res = ct.run.arangoshCmd(this.options, this.instanceManager,
                                     {}, [
                                       '--javascript.execute-string',
                                       `
          var users = require("@arangodb/users");
          users.save("replicator-user", "replicator-password", true);
          users.grantDatabase("replicator-user", "_system");
          users.grantCollection("replicator-user", "_system", "*", "rw");
          users.reload();
          `
                                     ],
                                     this.options.coreCheck);
        state = res.status;
      });
    }
    return {
      message: message,
      state: state,
    };
  }
};


// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: replication_static
// //////////////////////////////////////////////////////////////////////////////

function replicationStatic (options) {
  let testCases = tu.scanTestPaths(testPaths.replication_static, options);
  testCases = tu.splitBuckets(options, testCases);
  let localOptions = Object.assign({extraArgs: {'vector-index': true}}, options, tu.testServerAuthInfo);
  let ret = new replicationRunner(
    localOptions,
    'leader_static',
    {
      'server.authentication': 'true',
      'vector-index': 'true',
    }, true).run(testCases);
  options.cleanup = options.cleanup && localOptions.cleanup;
  return ret;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: replication_sync
// //////////////////////////////////////////////////////////////////////////////

function replicationSync (options) {
  let testCases = tu.scanTestPaths(testPaths.replication_sync, options);
  testCases = tu.splitBuckets(options, testCases);

  return new replicationRunner(options, 'replication_sync', {"server.authentication": "true"}).run(testCases);
}

exports.setup = function (testFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['shell_replication'] = shellReplication;
  testFns['replication_static'] = replicationStatic;
  testFns['replication_sync'] = replicationSync;
  testFns['http_replication'] = shellClientReplicationApi;
  tu.CopyIntoObject(fnDocs, functionsDocumentation);
  tu.CopyIntoList(optionsDoc, optionsDocumentation);
};
