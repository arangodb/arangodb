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

// const BLUE = require('internal').COLORS.COLOR_BLUE;
const CYAN = require('internal').COLORS.COLOR_CYAN;
const GREEN = require('internal').COLORS.COLOR_GREEN;
const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

const functionsDocumentation = {
  'permissions_server': 'permissions test for the server'
};

const testPaths = {
  'permissions_server': [tu.pathForTesting('server/permissions')]
};

function permissions_server(options) {
  let count = 0;
  let results = {};
  let filtered = {};
  const tests = tu.scanTestPaths(testPaths.permissions_server);

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
      let instanceInfo = pu.startInstance(options.protocol, options, paramsFistRun, "permissions_server", rootDir); // fist start
      pu.cleanupDBDirectoriesAppend(instanceInfo.rootDir);      
      try {
        let content = fs.read(testFile);
        content = `(function(){ const getOptions = true; ${content} 
}())`; // DO NOT JOIN WITH THE LINE ABOVE -- because of content could contain '//' at the very EOF

        paramsSecondRun = executeScript(content, true, testFile);
      } catch (ex) {
        results[testFile] = {
          status: false,
          messages: 'Warmup of system failed: ' + ex
        };
        pu.shutdownInstance(instanceInfo, clonedOpts, false);                                     // stop
        return;
      }

      pu.shutdownInstance(instanceInfo, clonedOpts, false);                                     // stop
      pu.reStartInstance(options, instanceInfo, paramsSecondRun);      // restart with restricted permissions
      results[testFile] = tu.runInLocalArangosh(options, instanceInfo, testFile, {});
      pu.shutdownInstance(instanceInfo, clonedOpts, false);
      if (!results[testFile].status) {
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
  testFns['permissions_server'] = permissions_server;
  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
};
