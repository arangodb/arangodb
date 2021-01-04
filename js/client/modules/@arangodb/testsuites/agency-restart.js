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
  'agency-restart': 'run recovery tests'
};
const optionsDocumentation = [
];

const fs = require('fs');
const pu = require('@arangodb/testutils/process-utils');
const tu = require('@arangodb/testutils/test-utils');
const _ = require('lodash');

const toArgv = require('internal').toArgv;

const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
const BLUE = require('internal').COLORS.COLOR_BLUE;

const testPaths = {
  'agency-restart': [tu.pathForTesting('server/agency-restart')]
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: agency_restart
// //////////////////////////////////////////////////////////////////////////////

function runArangodRecovery (params) {
  let additionalParams= {
    'log.foreground-tty': 'true',
    'javascript.enabled': 'true',
    'agency.activate': 'true',
    'agency.compaction-keep-size': '10000',
    'agency.wait-for-sync': 'false',
    'agency.size': '1',
  };

  let argv = [];

  let binary = pu.ARANGOD_BIN;
  let crashLogDir = fs.join(fs.getTempPath(), 'crash');
  fs.makeDirectoryRecursive(crashLogDir);
  pu.cleanupDBDirectoriesAppend(crashLogDir);

  let crashLog = fs.join(crashLogDir, 'crash.log');

  if (params.setup) {
    additionalParams['javascript.script-parameter'] = 'setup';
    try {
      // clean up crash log before next test
      fs.remove(crashLog);
    } catch (err) {}


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
      'database.directory': fs.join(dataDir + 'db'),
      'server.rest-server': 'false',
      'javascript.script': params.script
    });
      
    args['log.output'] = 'file://' + crashLog;
    params.args = args;

    argv = toArgv(Object.assign(params.args, additionalParams));
  } else {
    additionalParams['javascript.script-parameter'] = 'recory';
    argv = toArgv(Object.assign(params.args, additionalParams));
    
    if (params.options.rr) {
      binary = 'rr';
      argv.unshift(pu.ARANGOD_BIN);
    }
  }
    
  process.env["crash-log"] = crashLog;
  params.instanceInfo.pid = pu.executeAndWait(
    binary,
    argv,
    params.options,
    'recovery',
    params.instanceInfo.rootDir,
    !params.setup && params.options.coreCheck);
}

function agencyRestart (options) {
  if (!global.ARANGODB_CLIENT_VERSION(true)['failure-tests'] ||
      global.ARANGODB_CLIENT_VERSION(true)['failure-tests'] === 'false') {
    return {
      agencyRestart: {
        status: false,
        message: 'failure-tests not enabled. please recompile with -DUSE_FAILURE_TESTS=On'
      },
      status: false
    };
  }

  let results = {
    status: true
  };

  let agencyRestartTests = tu.splitBuckets(options, tu.scanTestPaths(testPaths['agency-restart'], options));
  let count = 0;
  let orgTmp = process.env.TMPDIR;
  let tempDir = fs.join(fs.getTempPath(), 'agency-restart');
  fs.makeDirectoryRecursive(tempDir);
  process.env.TMPDIR = tempDir;
  pu.cleanupDBDirectoriesAppend(tempDir);

  for (let i = 0; i < agencyRestartTests.length; ++i) {
    let test = agencyRestartTests[i];
    let filtered = {};

    if (tu.filterTestcaseByOptions(test, options, filtered)) {
      count += 1;
      ////////////////////////////////////////////////////////////////////////
      print(BLUE + "running setup of test " + count + " - " + test + RESET);
      let params = {
        tempDir: tempDir,
        instanceInfo: {
          rootDir: fs.join(fs.getTempPath(), 'agency-restart', count.toString())
        },
        options: _.cloneDeep(options),
        script: test,
        setup: true,
        count: count,
        testDir: ""
      };
      runArangodRecovery(params);

      ////////////////////////////////////////////////////////////////////////
      print(BLUE + "running recovery of test " + count + " - " + test + RESET);
      params.options.disableMonitor = options.disableMonitor;
      params.setup = false;
      try {
        tu.writeTestResult(params.args['temp.path'], {
          failed: 1,
          status: false, 
          message: "unable to run agency_restart test " + test,
          duration: -1
        });
      } catch (er) {}
      runArangodRecovery(params);

      results[test] = tu.readTestResult(
        params.args['temp.path'],
        {
          status: false
        },
        test
      );
      if (!results[test].status) {
        print("Not cleaning up " + params.testDir);
        results.status = false;
      }
      else {
        pu.cleanupLastDirectory(params.options);
      }
    } else {
      if (options.extremeVerbosity) {
        print('Skipped ' + test + ' because of ' + filtered.filter);
      }
    }
  }
  process.env.TMPDIR = orgTmp;
  if (count === 0) {
    print(RED + 'No testcase matched the filter.' + RESET);
    return {
      ALLTESTS: {
        status: true,
        skipped: true
      },
      status: true
    };
  }

  return results;
}

exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['agency-restart'] = agencyRestart;
  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
