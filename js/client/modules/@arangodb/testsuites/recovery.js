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
  let useEncryption = false;

  if (global.ARANGODB_CLIENT_VERSION) {
    let version = global.ARANGODB_CLIENT_VERSION(true);
    if (version.hasOwnProperty('enterprise-version')) {
      useEncryption = true;
    }
  }

  let argv = [];

  let binary = pu.ARANGOD_BIN;
  let crashLogDir = fs.join(fs.getTempPath(), 'crash');
  fs.makeDirectoryRecursive(crashLogDir);
  pu.cleanupDBDirectoriesAppend(crashLogDir);

  let crashLog = fs.join(crashLogDir, 'crash.log');

  if (params.setup) {
    try {
      // clean up crash log before next test
      fs.remove(crashLog);
    } catch (err) {}


    // special handling for crash-handler recovery tests
    if (params.script.match(/crash-handler/)) {
      // forcefully enable crash handler, even if turned off globally
      // during testing
      require('internal').env["ARANGODB_OVERRIDE_CRASH_HANDLER"] = "on";
    }

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
      'rocksdb.wal-file-timeout-initial': 10,
      'database.directory': fs.join(dataDir + 'db'),
      'server.rest-server': 'false',
      'replication.auto-start': 'true',
      'javascript.script': params.script
    });
      
    args['log.output'] = 'file://' + crashLog;

    if (useEncryption) {
      let keyDir = fs.join(fs.getTempPath(), 'arango_encryption');
      if (!fs.exists(keyDir)) {  // needed on win32
        fs.makeDirectory(keyDir);
      }
      pu.cleanupDBDirectoriesAppend(keyDir);
        
      const key = '01234567890123456789012345678901';
      
      let keyfile = fs.join(keyDir, 'rocksdb-encryption-keyfile');
      fs.write(keyfile, key);

      // special handling for encryption-keyfolder tests
      if (params.script.match(/encryption-keyfolder/)) {
        args['rocksdb.encryption-keyfolder'] = keyDir;
        process.env["rocksdb-encryption-keyfolder"] = keyDir;
      } else {
        args['rocksdb.encryption-keyfile'] = keyfile;
        process.env["rocksdb-encryption-keyfile"] = keyfile;
      }
    }

    params.args = args;

    argv = toArgv(
      Object.assign(params.args,
                    {
                      'log.foreground-tty': 'true',
                      'javascript.script-parameter': 'setup'
                    }
                   )
    );
  } else {
    argv = toArgv(
      Object.assign(params.args,
                    {
                      'log.foreground-tty': 'true',
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
    
  process.env["crash-log"] = crashLog;
  params.instanceInfo.pid = pu.executeAndWait(
    binary,
    argv,
    params.options,
    'recovery',
    params.instanceInfo.rootDir,
    !params.setup && params.options.coreCheck);
}

function recovery (options) {
  if (!global.ARANGODB_CLIENT_VERSION(true)['failure-tests'] ||
      global.ARANGODB_CLIENT_VERSION(true)['failure-tests'] === 'false') {
    return {
      recovery: {
        status: false,
        message: 'failure-tests not enabled. please recompile with -DUSE_FAILURE_TESTS=On'
      },
      status: false
    };
  }

  let results = {
    status: true
  };

  let recoveryTests = tu.scanTestPaths(testPaths.recovery, options);

  recoveryTests = tu.splitBuckets(options, recoveryTests);

  let count = 0;
  let orgTmp = process.env.TMPDIR;
  let tempDir = fs.join(fs.getTempPath(), 'recovery');
  fs.makeDirectoryRecursive(tempDir);
  process.env.TMPDIR = tempDir;
  pu.cleanupDBDirectoriesAppend(tempDir);

  for (let i = 0; i < recoveryTests.length; ++i) {
    let test = recoveryTests[i];
    let filtered = {};

    if (tu.filterTestcaseByOptions(test, options, filtered)) {
      count += 1;
      ////////////////////////////////////////////////////////////////////////
      print(BLUE + "running setup of test " + count + " - " + test + RESET);
      let params = {
        tempDir: tempDir,
        instanceInfo: {
          rootDir: fs.join(fs.getTempPath(), 'recovery', count.toString())
        },
        options: _.cloneDeep(options),
        script: test,
        setup: true,
        count: count,
        testDir: ""
      };

      for (let phase = 1; ; ++phase) {
        runArangodRecovery(params);

        // check if a CONTINUE file exists
        let continueFile = fs.join(params.args['temp.path'], "CONTINUE");
        // very likely the CONTINUE file does not exist.

        // the test we now run _may_ write a CONTINUE file.
        // if it does, we keep looping here until the test fails
        // or we don't have any CONTINUE file
        if (!fs.exists(continueFile)) {
          break;
        }
      }

      ////////////////////////////////////////////////////////////////////////
      print(BLUE + "running recovery of test " + count + " - " + test + RESET);
      params.options.disableMonitor = options.disableMonitor;
      params.setup = false;
      try {
        tu.writeTestResult(params.args['temp.path'], {
          failed: 1,
          status: false, 
          message: "unable to run recovery test " + test,
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
  testFns['recovery'] = recovery;
  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
