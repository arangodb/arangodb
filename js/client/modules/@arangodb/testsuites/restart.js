/* jshint strict: false, sub: true */
/* global print, params, arango */
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
// / @author Wilfried Goesgens
// //////////////////////////////////////////////////////////////////////////////

const _ = require('lodash');
const time = require('internal').time;
const fs = require('fs');
const yaml = require('js-yaml');

const pu = require('@arangodb/process-utils');
const tu = require('@arangodb/test-utils');

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

function runTest (options, instanceInfo, file, addArgs) {
  let endpoint = arango.getEndpoint();
  if (( options.vst && endpoint !== instanceInfo.vstEndpoint) ||
      (!options.vst && endpoint !== instanceInfo.endpoint)) {
    let newEndpoint = (options.vst && instanceInfo.hasOwnProperty('vstEndpoint')) ?
        instanceInfo.vstEndpoint : 
        instanceInfo.endpoint;
    print(`runInLocalArangosh: Reconnecting to ${newEndpoint} from ${endpoint}`);
    arango.reconnect(newEndpoint, '_system', 'root', '');
  }
  
  let testCode;
  // \n's in testCode are required because of content could contain '//' at the very EOF
  if (file.indexOf('-spec') === -1) {
    let testCase = JSON.stringify(options.testCase);
    if (options.testCase === undefined) {
      testCase = '"undefined"';
    }
    testCode = 'const runTest = require("jsunity").runTest;\n ' +
      'return runTest(' + JSON.stringify(file) + ', true, ' + testCase + ');\n';
  } else {
    let mochaGrep = options.testCase ? ', ' + JSON.stringify(options.testCase) : '';
    testCode = 'const runTest = require("@arangodb/mocha-runner"); ' +
      'return runTest(' + JSON.stringify(file) + ', true' + mochaGrep + ');\n';
  }

  global.instanceInfo = instanceInfo;
  global.testOptions = options;
  let testFunc;
  eval('testFunc = function () { ' + testCode + " \n}");
  
  try {
    let result = testFunc();
    return result;
  } catch (ex) {
    return {
      status: false,
      message: "test has thrown! '" + file + "' - " + ex.message || String(ex),
      stack: ex.stack
    };
  }
}

function restart (options) {
  let clonedOpts = _.clone(options);
  clonedOpts.disableClusterMonitor = true;
  clonedOpts.skipLogAnalysis = true;
  clonedOpts.skipReconnect = true;
  let testCases = tu.scanTestPaths(testPaths.restart, clonedOpts);
  return tu.performTests(clonedOpts, testCases, 'restart', runTest, {
    'server.jwt-secret': 'haxxmann',
  });
}

exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['restart'] = restart;
  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
};
