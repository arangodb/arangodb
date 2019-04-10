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
const optionsDocumentation = [
  '   - `skipShebang`: if set, the shebang tests are skipped.'
];

const testPaths = {
  'permissions_server': [tu.pathForTesting('server/permissions')]
};

function permissions_server(options) {

  // pass on JWT secret
  let clonedOpts = _.clone(options);
  let serverOptions = {};
  if (serverOptions['server.jwt-secret'] && !clonedOpts['server.jwt-secret']) {
    clonedOpts['server.jwt-secret'] = serverOptions['server.jwt-secret'];
  }

  let paramsFistRun = {};
  let paramsSecondRun = {'javascript.allow-port-testing':false };

  let instanceInfo = pu.startInstance(options.protocol, options, paramsFistRun, "permissions_server"); // fist start
  // geiles setup
  pu.shutdownInstance(instanceInfo, clonedOpts, false);                                     // stop
  pu.reStartInstance(options, instanceInfo, paramsSecondRun);      // restart with restricted permissions
  // geile tests
  pu.shutdownInstance(instanceInfo, clonedOpts, false);
  return  { failed: 0 };
}

function doxx_xxx(options) {
  let argv = [];
  let binary = pu.ARANGOD_BIN;

  if (params.setup) {
    params.options.disableMonitor = true;
    params.testDir = fs.join(params.tempDir, `${params.count}`);
    pu.cleanupDBDirectoriesAppend(params.testDir);
    let dataDir = fs.join(params.testDir, 'data');
    let appDir = fs.join(params.testDir, 'app');
    let tmpDir = fs.join(params.testDir, 'tmp');
    fs.makeDirectoryRecursive(params.testDir);
    fs.makeDirectoryRecursive(dataDir);
    fs.makeDirectoryRecursive(tmpDir);
    fs.makeDirectoryRecursive(appDir);

    let args = pu.makeArgs.arangod(params.options, appDir, '', tmpDir);
    // enable development debugging if extremeVerbosity is set
    if (params.options.extremeVerbosity === true) {
      args['log.level'] = 'development=info';
    }
    args = Object.assign(args, params.options.extraArgs);
    args = Object.assign(args, {
      'wal.reserve-logfiles': 1,
      'rocksdb.wal-file-timeout-initial': 10,
      'database.directory': fs.join(dataDir + 'db'),
      'server.rest-server': 'false',
      'replication.auto-start': 'true',
      'javascript.script': params.script
    });
    params.args = args;

    argv = toArgv(
      Object.assign(params.args,
                    {
                      'javascript.script-parameter': 'setup'
                    }
                   )
    );
  } else {
    argv = toArgv(
      Object.assign(params.args,
                    {
                      'log.foreground-tty': 'true',
                      'wal.ignore-logfile-errors': 'true',
                      'database.ignore-datafile-errors': 'false', // intentionally false!
                      'javascript.script-parameter': 'recovery'
                    }
                   )
    );
    if (params.options.rr) {
      binary = 'rr';
      argv.unshift(pu.ARANGOD_BIN);
    }
  }
  params.instanceInfo.pid = pu.executeAndWait(
    binary,
    argv,
    params.options,
    'recovery',
    params.instanceInfo.rootDir,
    params.setup,
    !params.setup && params.options.coreCheck);





  let res = {};
  let filtered = {};
  let rootDir = fs.join(fs.getTempPath(), 'permissions');
  const tests = tu.scanTestPaths(testPaths.permissions);

  fs.makeDirectoryRecursive(rootDir);

  tests.forEach(function (f, i) {
    if (tu.filterTestcaseByOptions(f, options, filtered)) {
      let content = fs.read(f);
      content = `(function(){ const getOptions = true; ${content}
}())`; // DO NOT JOIN WITH THE LINE ABOVE -- because of content could contain '//' at the very EOF

      let testOptions = executeScript(content, true, f);
      res[f] = tu.runInArangosh(options,
                                {
                                  endpoint: 'tcp://127.0.0.1:8888',
                                  rootDir: rootDir
                                },
                                f,
                                testOptions
                               );
    } else {
      if (options.extremeVerbosity) {
        print('Skipped ' + f + ' because of ' + filtered.filter);
      }
    }

  });
  return res;
}

exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['permissions_server'] = permissions_server;
  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
