/* jshint strict: false, sub: true */
/* global print, params */
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
  'server_permissions': 'permissions test for the server'
};

const testPaths = {
  'server_permissions': [tu.pathForTesting('client/server_permissions')]
};

function server_permissions(options) {
  let count = 0;
  let results = { shutdown: true };
  let filtered = {};
  const tests = tu.scanTestPaths(testPaths.server_permissions, options);

  tests.forEach(function (testFile, i) {
    count += 1;
    if (tu.filterTestcaseByOptions(testFile, options, filtered)) {
      // pass on JWT secret
      let clonedOpts = _.clone(options);
      let serverOptions = {};
      if (serverOptions['server.jwt-secret'] && !clonedOpts['server.jwt-secret']) {
        clonedOpts['server.jwt-secret'] = serverOptions['server.jwt-secret'];
      }

      let paramsFistRun = {};
      let paramsSecondRun;
      let rootDir = fs.join(fs.getTempPath(), count.toString());
      let instanceInfo = pu.startInstance(options.protocol, options, paramsFistRun, "server_permissions", rootDir); // fist start
      pu.cleanupDBDirectoriesAppend(instanceInfo.rootDir);      
      try {
        print(BLUE + '================================================================================' + RESET);
        print(CYAN + 'Running Setup of: ' + testFile + RESET);
        let content = fs.read(testFile);
        content = `(function(){ const getOptions = true; ${content} 
}())`; // DO NOT JOIN WITH THE LINE ABOVE -- because of content could contain '//' at the very EOF

        paramsSecondRun = executeScript(content, true, testFile);
      } catch (ex) {
        let shutdownStatus = pu.shutdownInstance(instanceInfo, clonedOpts, false);                                     // stop
        results[testFile] = {
          status: false,
          messages: 'Warmup of system failed: ' + ex,
          shutdown: shutdownStatus
        };
        results['shutdown'] = results['shutdown'] && shutdownStatus;
        return;
      }

      if (paramsSecondRun.hasOwnProperty('server.jwt-secret')) {
        clonedOpts['server.jwt-secret'] = paramsSecondRun['server.jwt-secret'];
      }
      let shutdownStatus = pu.shutdownInstance(instanceInfo, clonedOpts, false);                                     // stop
      if (shutdownStatus) {
        pu.reStartInstance(clonedOpts, instanceInfo, paramsSecondRun);      // restart with restricted permissions
        results[testFile] = tu.runInLocalArangosh(options, instanceInfo, testFile, {});
        shutdownStatus = pu.shutdownInstance(instanceInfo, clonedOpts, false);
      }
      else {
        results[testFile] = {
          status: false,
          message: "failed to stop instance",
          shutdown: false
        };
      }
      results['shutdown'] = results['shutdown'] && shutdownStatus;
      
      if (!results[testFile].status || !shutdownStatus) {
        print("Not cleaning up " + instanceInfo.rootDir);
        results.status = false;
      }
      else {
        pu.cleanupLastDirectory(options);
      }
    } else {
      if (options.extremeVerbosity) {
        print('Skipped ' + testFile + ' because of ' + filtered.filter);
      }
    }
  });
  return results;
}

exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['server_permissions'] = server_permissions;
  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
};
