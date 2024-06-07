/* jshint strict: false, sub: true */
/* global print */
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
// / @author Max Neunhoeffer
// //////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'recovery_server': 'run recovery server tests'
};

const fs = require('fs');
const pu = require('@arangodb/testutils/process-utils');
const tu = require('@arangodb/testutils/test-utils');
const trs = require('@arangodb/testutils/testrunners');
const inst = require('@arangodb/testutils/instance');
const _ = require('lodash');
const tmpDirMmgr = require('@arangodb/testutils/tmpDirManager').tmpDirManager;

const toArgv = require('internal').toArgv;
const { isEnterprise, versionHas } = require("@arangodb/test-helper");

const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
const BLUE = require('internal').COLORS.COLOR_BLUE;

const testPaths = {
  'recovery_server': [tu.pathForTesting('server/recovery')]
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: recovery_server
// //////////////////////////////////////////////////////////////////////////////

function runArangodRecovery (params, useEncryption, exitSuccessOk, exitFailOk) {
  let additionalParams= {
    'log.foreground-tty': 'true',
    'database.ignore-datafile-errors': 'false', // intentionally false!
  };

  if (useEncryption) {
    // randomly turn on or off hardware-acceleration for encryption for both
    // setup and the actual test. given enough tests, this will ensure that we run
    // a good mix of accelerated and non-accelerated encryption code. in addition,
    // we shuffle between the setup and the test phase, so if there is any
    // incompatibility between the two modes, this will likely find it
    additionalParams['rocksdb.encryption-hardware-acceleration'] = (Math.random() * 100 >= 50) ? "true" : "false";
  }

  let argv = [];

  let binary = pu.ARANGOD_BIN;
  if (params.setup) {
    additionalParams['javascript.script-parameter'] = 'setup';

    // special handling for crash-handler recovery tests
    if (params.script.match(/crash-handler/)) {
      // forcefully enable crash handler, even if turned off globally
      // during testing
      require('internal').env["ARANGODB_OVERRIDE_CRASH_HANDLER"] = "on";
    }

    // enable development debugging if extremeVerbosity is set
    let args = Object.assign({
      'rocksdb.wal-file-timeout-initial': 10,
      'server.rest-server': 'false',
      'replication.auto-start': 'true',
      'javascript.script': params.script,
      'log.output': 'file://' + params.crashLog
    }, params.options.extraArgs);
    
    if (params.options.extremeVerbosity === true) {
      args['log.level'] = 'development=info';
    }

    if (useEncryption) {
      const key = '01234567890123456789012345678901';
      
      let keyfile = fs.join(params.keyDir, 'rocksdb-encryption-keyfile');
      fs.write(keyfile, key);

      // special handling for encryption-keyfolder tests
      if (params.script.match(/encryption-keyfolder/)) {
        args['rocksdb.encryption-keyfolder'] = params.keyDir;
        process.env["rocksdb-encryption-keyfolder"] = params.keyDir;
      } else {
        args['rocksdb.encryption-keyfile'] = keyfile;
        process.env["rocksdb-encryption-keyfile"] = keyfile;
      }
    }
    params.options.disableMonitor = true;
    params.testDir = fs.join(params.tempDir, `${params.count}`);
    params['instance'] = new inst.instance(params.options,
                                           inst.instanceRole.single,
                                           args, {}, 'tcp', params.testDir, '',
                                           new inst.agencyConfig(params.options, null));

    argv = toArgv(Object.assign(params.instance.args, additionalParams));
  } else {
    additionalParams['javascript.script-parameter'] = 'recovery';
    argv = toArgv(Object.assign(params.instance.args, additionalParams));
    
    if (params.options.rr) {
      binary = 'rr';
      argv.unshift(pu.ARANGOD_BIN);
    }
  }
  
  process.env["state-file"] = params.stateFile;
  process.env["crash-log"] = params.crashLog;
  process.env["isSan"] = params.options.isSan;
  params.instanceInfo.pid = pu.executeAndWait(
    binary,
    argv,
    params.options,
    false,
    params.instance.rootDir,
    !params.setup && params.options.coreCheck,
    0,
    params.instanceInfo);
  if (params.setup) {
    const hasSignal = params.instanceInfo.exitStatus.hasOwnProperty('signal');
    const hasExitZero = !hasSignal && params.instanceInfo.exitStatus.exit === 0;
    const hasExitOne = !hasSignal && params.instanceInfo.exitStatus.exit === 1;
    if (exitFailOk) {
      if (!hasExitOne) {
        return {
          status: false,
          timeout: false,
          message: `setup of test didn't exit 1 as expected: ${JSON.stringify(params.instanceInfo.exitStatus)}`
        };
      }
    } else if (exitSuccessOk) {
      if (!hasExitZero) {
        return {
          status: false,
          timeout: false,
          message: `setup of test didn't exit success as expected: ${JSON.stringify(params.instanceInfo.exitStatus)}`
        };
      }
    } else if (!hasSignal) {
      return {
        status: false,
        timeout: false,
        message: `setup of test didn't crash as expected: ${JSON.stringify(params.instanceInfo.exitStatus)}`
      };
    }
  }
  return {
    status: true
  };
}

function recovery_server (options) {
  if (!versionHas('failure-tests')) {
    return {
      recovery_server: {
        status: false,
        message: 'failure-tests not enabled. please recompile with -DUSE_FAILURE_TESTS=On'
      },
      status: false
    };
  }

  let results = {
    status: true
  };

  let useEncryption = isEnterprise();

  let recoveryTests = tu.scanTestPaths(testPaths.recovery_server, options);

  recoveryTests = tu.splitBuckets(options, recoveryTests);
  let count = 0;

  let tmpMgr = new tmpDirMmgr('recovery', options);

  for (let i = 0; i < recoveryTests.length; ++i) {
    let test = recoveryTests[i];
    let filtered = {};

    if (tu.filterTestcaseByOptions(test, options, filtered)) {
      count += 1;
      let iteration = 0;
      let stateFile = fs.getTempFile();
      let exitSuccessOk = test.indexOf('-exitzero') >= 0;
      let exitFailOk = test.indexOf('-exitone') >= 0;

      while (true) {
        ++iteration;
        print(BLUE + "running setup #" + iteration + " of test " + count + " - " + test + RESET);
        let params = {
          tempDir: tmpMgr.tempDir,
          instanceInfo: {
            rootDir: fs.join(fs.getTempPath(), 'recovery', count.toString())
          },
          options: _.cloneDeep(options),
          script: test,
          setup: true,
          count: count,
          testDir: "",
          stateFile,
          crashLogDir: fs.join(fs.getTempPath(), `crash_${count}`),
          crashLog: "",
          keyDir: ""          
        };
        fs.makeDirectoryRecursive(params.crashLogDir);
        params.crashLog = fs.join(params.crashLogDir, 'crash.log');
        if (useEncryption) {
          params.keyDir = fs.join(fs.getTempPath(), `arango_encryption_${count}`);
          fs.makeDirectory(params.keyDir);
        }
        let res = runArangodRecovery(params, useEncryption, exitSuccessOk, exitFailOk);
        if (!res.status) {
          results[test] = res;
          break;
        }
          
        ////////////////////////////////////////////////////////////////////////
        print(BLUE + "running recovery_server #" + iteration + " of test " + count + " - " + test + RESET);
        params.options.disableMonitor = options.disableMonitor;
        params.setup = false;
        try {
          trs.writeTestResult(params.instance.args['temp.path'], {
            failed: 1,
            status: false, 
            message: "unable to run recovery_server test " + test,
            duration: -1
          });
        } catch (er) {}
        runArangodRecovery(params, useEncryption, exitSuccessOk, exitFailOk);

        results[test] = trs.readTestResult(
          params.instance.args['temp.path'],
          {
            status: false
          },
          test
        );
        if (!results[test].status) {
          print("Not cleaning up " + params.testDir);
          results.status = false;
          // end while loop
          break;
        } 

        // check if the state file has been written by the test.
        // if so, we will run another round of this test!
        try {
          if (String(fs.readFileSync(stateFile)).length) {
            print('Going into next iteration of recovery_server test');
            if (params.options.cleanup) {
              if (params.crashLogDir !== "") {
                fs.removeDirectoryRecursive(params.crashLogDir, true);
              }
              if (params.keyDir !== "") {
                fs.removeDirectoryRecursive(params.keyDir, true);
              }
              continue;
            }
          }
        } catch (err) {
        }
        // last iteration. break out of while loop
        if (params.options.cleanup) {
          if (params.crashLogDir !== "") {
            fs.removeDirectoryRecursive(params.crashLogDir, true);
          }
          if (params.keyDir !== "") {
            fs.removeDirectoryRecursive(params.keyDir, true);
          }
          params.instance.cleanup();
        }
        break;
      }
    } else {
      if (options.extremeVerbosity) {
        print('Skipped ' + test + ' because of ' + filtered.filter);
      }
    }
  }
  tmpMgr.destructor(options.cleanup && results.status);
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

exports.setup = function (testFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['recovery_server'] = recovery_server;
  tu.CopyIntoObject(fnDocs, functionsDocumentation);
};
