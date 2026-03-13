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
// / @author Max Neunhoeffer
// //////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'shell_replication': 'shell replication tests',
  'http_replication': 'client replication API tests'
};
const optionsDocumentation = [
];

const _ = require('lodash');
const tu = require('@arangodb/testutils/test-utils');
const trs = require('@arangodb/testutils/testrunners');

const testPaths = {
  'shell_replication': [tu.pathForTesting('common/replication')],
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
// / @brief TEST: http_replication
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

exports.setup = function (testFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['shell_replication'] = shellReplication;
  testFns['http_replication'] = shellClientReplicationApi;
  tu.CopyIntoObject(fnDocs, functionsDocumentation);
  tu.CopyIntoList(optionsDoc, optionsDocumentation);
};
