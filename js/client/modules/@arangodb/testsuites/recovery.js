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

const toArgv = require('internal').toArgv;

const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: recovery
// //////////////////////////////////////////////////////////////////////////////

function runArangodRecovery (instanceInfo, options, script, setup) {
  if (!instanceInfo.tmpDataDir) {
    let td = fs.join(fs.getTempFile(), 'data');
    fs.makeDirectoryRecursive(td);

    instanceInfo.tmpDataDir = td;
  }

  if (!instanceInfo.recoveryArgs) {
    let args = pu.makeArgs.arangod(options);
    args['server.threads'] = 1;
    args['wal.reserve-logfiles'] = 1;
    args['database.directory'] = instanceInfo.tmpDataDir;

    instanceInfo.recoveryArgv = toArgv(args).concat(['--server.rest-server', 'false']);
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
  if (setup) {
    binary = pu.TOP_DIR + '/scripts/disable-cores.sh';
    argv.unshift(pu.ARANGOD_BIN);
  }

  instanceInfo.pid = pu.executeAndWait(binary, argv, options, 'recovery', instanceInfo.rootDir, setup);
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

  let recoveryTests = tu.scanTestPath('js/server/tests/recovery');
  let count = 0;

  for (let i = 0; i < recoveryTests.length; ++i) {
    let test = recoveryTests[i];
    let filtered = {};

    if (tu.filterTestcaseByOptions(test, options, filtered)) {
      let instanceInfo = {};
      count += 1;

      runArangodRecovery(instanceInfo, options, test, true);

      runArangodRecovery(instanceInfo, options, test, false);

      if (instanceInfo.tmpDataDir) {
        fs.removeDirectoryRecursive(instanceInfo.tmpDataDir, true);
      }

      results[test] = instanceInfo.pid;

      if (!results[test].status) {
        status = false;
      }
    } else {
      if (options.extremeVerbosity) {
        print('Skipped ' + test + ' because of ' + filtered.filter);
      }
    }
  }

  if (count === 0) {
    results['ALLTESTS'] = {
      status: false,
      skipped: true
    };
    status = false;
    print(RED + 'No testcase matched the filter.' + RESET);
  }
  results.status = status;

  return {
    recovery: results
  };
}

function setup (testFns, defaultFns, opts, fnDocs, optionsDoc) {
  testFns['recovery'] = recovery;
  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
}

exports.setup = setup;
