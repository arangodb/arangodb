/* jshint strict: true */
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License")
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'replication2_client': 'tests for the replication2 http api',
  'replication2_server': 'tests for the replication2',
};
const optionsDocumentation = [];

const _ = require('lodash');
const tu = require('@arangodb/testutils/test-utils');

const testPaths = {
  'replication2_client': [tu.pathForTesting('client/replication2')],
  'replication2_server': [tu.pathForTesting('server/replication2')],
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: replication2_client
////////////////////////////////////////////////////////////////////////////////

function replication2Client(options) {
  const testCases = tu.scanTestPaths(testPaths.replication2_client, options);

  const opts = _.clone(options);
  opts.dbServers = Math.max(opts.dbServers, 3);

  return tu.performTests(opts, testCases, 'replication2_client', tu.runInLocalArangosh);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: replication2_server
////////////////////////////////////////////////////////////////////////////////

function replication2Server(options) {
  const testCases = tu.scanTestPaths(testPaths.replication2_server, options);

  const opts = _.clone(options);
  opts.dbServers = Math.max(opts.dbServers, 5);

  return tu.performTests(opts, testCases, 'replication2_server', tu.runThere, {
        'javascript.allow-external-process-control': 'true',
        'javascript.allow-port-testing': 'true',
        'javascript.allow-admin-execute': 'true',
        'agency.supervision-grace-period': '3.0',
        'agency.supervision-ok-threshold': '1.5',
      });
}


exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns.replication2_client = replication2Client;
  testFns.replication2_server = replication2Server;
  for (const [key, value] of Object.entries(functionsDocumentation)) {
    fnDocs[key] = value;
  }
  for (let i = 0; i < optionsDocumentation.length; i++) {
    optionsDoc.push(optionsDocumentation[i]);
  }
};
