/* jshint strict: false, sub: true */
/* global print, params, arango */
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
// / @author Wilfried Goesgens
// //////////////////////////////////////////////////////////////////////////////

const _ = require('lodash');
const time = require('internal').time;
const fs = require('fs');
const yaml = require('js-yaml');

const pu = require('@arangodb/testutils/process-utils');
const tu = require('@arangodb/testutils/test-utils');
const tr = require('@arangodb/testutils/testrunner');
const trs = require('@arangodb/testutils/testrunners');

const toArgv = require('internal').toArgv;
const executeScript = require('internal').executeScript;
const executeExternalAndWait = require('internal').executeExternalAndWait;

const platform = require('internal').platform;

const BLUE = require('internal').COLORS.COLOR_BLUE;
const CYAN = require('internal').COLORS.COLOR_CYAN;
const GREEN = require('internal').COLORS.COLOR_GREEN;
const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

const functionsDocumentation = {
  'restart': 'restart test for the server'
};

const testPaths = {
  'restart': [tu.pathForTesting('client/restart')]
};

class broadcastInstance extends trs.runLocalInArangoshRunner {
  constructor(options, testname, ...optionalArgs) {
    super(options, testname, ...optionalArgs);
    this.info = "localArangosh";
  }
  postStart() {
    global.theInstanceManager = this.instanceManager;
    return { state: true };
  }
}

function restart (options) {
  let clonedOpts = _.clone(options);
  clonedOpts.disableClusterMonitor = true;
  clonedOpts.skipLogAnalysis = true;
  clonedOpts.skipReconnect = true;
  if (clonedOpts.cluster && clonedOpts.coordinators < 2) {
    clonedOpts.coordinators = 2;
  }
  let testCases = tu.scanTestPaths(testPaths.restart, clonedOpts);
  global.obj = new broadcastInstance(
    clonedOpts, 'restart', Object.assign(
    {},
    tu.testServerAuthInfo, {
      'server.authentication': false
    }),
    tr.sutFilters.checkUsers.concat(tr.sutFilters.checkCollections));
  let rc = global.obj.run(testCases);
  options.cleanup = options.cleanup && clonedOpts.cleanup;
  return rc;
}

exports.setup = function (testFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['restart'] = restart;
  tu.CopyIntoObject(fnDocs, functionsDocumentation);
};
