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
  'recovery': 'run recovery tests'
};
const optionsDocumentation = [
];

const fs = require('fs');
const pu = require('@arangodb/process-utils');
const tu = require('@arangodb/test-utils');
const _ = require('lodash');

const toArgv = require('internal').toArgv;

const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
const BLUE = require('internal').COLORS.COLOR_BLUE;

const testPaths = {
  'recovery': [tu.pathForTesting('server/recovery')]
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: recovery
// //////////////////////////////////////////////////////////////////////////////

function runArangodRecovery (params) {
  let argv = [];

  if (params.setup) {
    params.options.disableMonitor = true;
    params.td = fs.join(params.tempDir, `${params.count}`);
    pu.cleanupDBDirectoriesAppend(params.td);
    let dataDir = fs.join(params.td, 'data');
    fs.makeDirectoryRecursive(params.td);
    let appDir = fs.join(params.td, 'app');
    fs.makeDirectoryRecursive(appDir);
    let tmpDir = fs.join(params.td, 'tmp');
    fs.makeDirectoryRecursive(tmpDir);

    let args = pu.makeArgs.arangod(params.options, appDir, '', tmpDir);
    // enable development debugging if extremeVerbosity is set
    if (params.options.extremeVerbosity === true) {
      args['log.level'] = 'development=info';
    }
    args = Object.assign(args, params.options.extraArgs);
    args = Object.assign(args, {
      'wal.reserve-logfiles': 1,
      'rocksdb.wal-file-timeout-initial': 10,
      'database.directory': dataDir + '/db',
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
                      'wal.ignore-logfile-errors': 'true',
                      'javascript.script-parameter': 'recovery'
                    }
                   )
    );
  }
  params.instanceInfo.pid = pu.executeAndWait(
    pu.ARANGOD_BIN,
    argv,
    params.options,
    'recovery',
    params.instanceInfo.rootDir,
    params.setup,
    !params.setup && params.options.coreCheck);
}

function recovery (options) {
  let results = {};

  if (!global.ARANGODB_CLIENT_VERSION(true)['failure-tests'] ||
      global.ARANGODB_CLIENT_VERSION(true)['failure-tests'] === 'false') {
    results.recovery = {
      status: false,
      message: 'failure-tests not enabled. please recompile with -DUSE_FAILURE_TESTS=On'
    };
    return results;
  }

  let recoveryTests = tu.scanTestPaths(testPaths.recovery);

  recoveryTests = tu.splitBuckets(options, recoveryTests);

  let count = 0;
  let orgTmp = process.env.TMPDIR;
  let tempDir = fs.join(fs.getTempPath(), 'crashtmp');
  fs.makeDirectoryRecursive(tempDir);
  process.env.TMPDIR = tempDir;
  pu.cleanupDBDirectoriesAppend(tempDir);

  for (let i = 0; i < recoveryTests.length; ++i) {
    let test = recoveryTests[i];
    let filtered = {};
    let localOptions = _.cloneDeep(options);
    let td="";

    if (tu.filterTestcaseByOptions(test, localOptions, filtered)) {
      let instanceInfo = {
        rootDir: pu.UNITTESTS_DIR
      };
      count += 1;

      print(BLUE + "running setup of test " + count + " - " + test + RESET);
      let params = {
        tempDir: tempDir,
        instanceInfo: instanceInfo,
        options: localOptions,
        script: test,
        setup: true,
        count: count,
        td: td
      };
      runArangodRecovery(params);
      params.options.disableMonitor = options.disableMonitor;
      params.setup = false;

      print(BLUE + "running recovery of " + test + RESET);
      runArangodRecovery(params);
      
      let jsonFN = fs.join(params.args['temp.path'], 'testresult.json');
      try {
        results[test] = JSON.parse(fs.read(jsonFN));
        fs.remove(jsonFN);
      } catch (x) {
        print(RED + 'failed to read ' + jsonFN + RESET);
        results[test] = {
          status: false,
          message: 'failed to read ' + jsonFN + x,
          duration: -1
        };
      }

      if (!results[test].status) {
        print("Not cleaning up " + params.td);
        results.status = false;
      }
      else {
        options.cleanup = false;
        pu.cleanupLastDirectory(localOptions);
      }
    } else {
      if (options.extremeVerbosity) {
        print('Skipped ' + test + ' because of ' + filtered.filter);
      }
    }
  }
  process.env.TMPDIR = orgTmp;
  if (count === 0) {
    results['ALLTESTS'] = {
      status: false,
      skipped: true
    };
    results.status = false;
    print(RED + 'No testcase matched the filter.' + RESET);
  }

  return results;
}

exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['recovery'] = recovery;
  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
