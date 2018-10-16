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

function runArangodRecovery (instanceInfo, options, script, setup, count) {
  let tmpDir;
  if (!instanceInfo.recoveryArgs) {
    let tempDir = fs.getTempPath();
    let td = fs.join(tempDir, `${count}`);
    if (setup) {
      pu.cleanupDBDirectoriesAppend(td);
    }
    td = fs.join(td, 'data');
    fs.makeDirectoryRecursive(td);
    instanceInfo.tmpDataDir = td;

    let appDir = fs.join(td, 'app');
    fs.makeDirectoryRecursive(appDir);
    tmpDir = fs.join(td, 'tmp');
    fs.makeDirectoryRecursive(tmpDir);

    let args = pu.makeArgs.arangod(options, appDir, '', tmpDir);
    args['wal.reserve-logfiles'] = 1;
    args['rocksdb.wal-file-timeout-initial'] = 10;
    args['database.directory'] = instanceInfo.tmpDataDir + '/db';

    args = Object.assign(args, options.extraArgs);

    instanceInfo.recoveryArgv = toArgv(args).concat(['--server.rest-server', 'false', '--replication.auto-start', 'true']);
  }

  let argv = instanceInfo.recoveryArgv;

  if (setup) {
    argv = argv.concat([
      '--javascript.script-parameter', 'setup'
    ]);
  } else {
    argv = argv.concat([
      '--wal.ignore-logfile-errors', 'true',
      '--javascript.script-parameter', 'recovery'
    ]);
  }

  // enable development debugging if extremeVerbosity is set
  if (options.extremeVerbosity === true) {
    argv = argv.concat([
      '--log.level', 'development=info'
    ]);
  }

  argv = argv.concat([
    '--javascript.script', script
  ]);

  let binary = pu.ARANGOD_BIN;

  instanceInfo.pid = pu.executeAndWait(binary, argv, options, 'recovery', instanceInfo.rootDir, setup, !setup && options.coreCheck);
  if (!setup) {
    let jsonFN = fs.join(tmpDir, 'testresult.json');
    try {
      let result = JSON.parse(fs.read(jsonFN));
      fs.remove(jsonFN);
      return result;
    } catch (x) {
      print(RED + 'failed to read ' + jsonFN + RESET);
      return {
        status: false,
        message: 'failed to read ' + jsonFN + x,
        duration: -1
      };
    }
  }
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

  let status = true;

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

    if (tu.filterTestcaseByOptions(test, localOptions, filtered)) {
      let instanceInfo = {};
      count += 1;

      print(BLUE + "running setup of " + test + RESET);
      runArangodRecovery(instanceInfo, localOptions, test, true, count);

      print(BLUE + "running recovery of " + test + RESET);
      results[test] = runArangodRecovery(instanceInfo, localOptions, test, false, count);

      if (!results[test].status) {
        status = false;
      }
      else {
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
    status = false;
    print(RED + 'No testcase matched the filter.' + RESET);
  }
  results.status = status;

  return results;
}

exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['recovery'] = recovery;
  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
