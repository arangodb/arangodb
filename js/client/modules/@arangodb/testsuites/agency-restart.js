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
// //////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'agency-restart': 'run recovery tests'
};

const fs = require('fs');
const tu = require('@arangodb/testutils/test-utils');
const cr = require('@arangodb/testutils/crash-recovery');
const _ = require('lodash');

const testPaths = {
  'agency-restart': [tu.pathForTesting('client/agency-restart')]
};

const agentArgs = {
  // agents have no users, so a login with credentials would fail
  'server.authentication': false,
  'agency.compaction-keep-size': '10000',
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: agency_restart
// / each test crashes and restarts a single agent
// //////////////////////////////////////////////////////////////////////////////

function agencyRestart (options) {
  const unsupported = cr.missingBuildSupport('agencyRestart');
  if (unsupported !== null) {
    return unsupported;
  }

  let agencyOptions = _.clone(options);
  agencyOptions.agency = true;
  agencyOptions.cluster = false;
  agencyOptions.agencySize = 1;

  return cr.runCrashRecoveryTests(
    agencyOptions,
    tu.scanTestPaths(testPaths['agency-restart'], agencyOptions),
    (localOptions, test, count) => cr.runCrashRecoveryCycle(
      localOptions, test, agentArgs, fs.join('agency-restart', count.toString())));
}

exports.setup = function (testFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['agency-restart'] = agencyRestart;
  tu.CopyIntoObject(fnDocs, functionsDocumentation);
};
