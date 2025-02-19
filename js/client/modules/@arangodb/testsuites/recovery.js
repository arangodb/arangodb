/* jshint strict: false, sub: true */
/* global print, arango */
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
/// @author Andrei Lobov
// //////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'recovery': 'run recovery tests',
  'recovery_cluster': 'run recovery cluster tests'
};

const fs = require('fs');
const internal = require('internal');
const executeScript = internal.executeScript;
const pu = require('@arangodb/testutils/process-utils');
const tu = require('@arangodb/testutils/test-utils');
const trs = require('@arangodb/testutils/testrunners');
const ct = require('@arangodb/testutils/client-tools');
const im = require('@arangodb/testutils/instance-manager');
const inst = require('@arangodb/testutils/instance');
const tmpDirMmgr = require('@arangodb/testutils/tmpDirManager').tmpDirManager;
const _ = require('lodash');
const { isEnterprise, versionHas } = require("@arangodb/test-helper");

const toArgv = internal.toArgv;
const ArangoError = require('@arangodb').ArangoError;

const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
const BLUE = require('internal').COLORS.COLOR_BLUE;

const termSignal = 15;

// At the moment only view-tests supported by cluster recovery tests:
const testPaths = {
  'recovery': [tu.pathForTesting('client/recovery')],
  'recovery_cluster': [tu.pathForTesting('client/recovery/search')]
};

// These tests should NOT be killed after the setup phase.
const doNotKillTests = [
  "tests/js/client/recovery/check-warnings-exitzero.js"
];

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: recovery
// //////////////////////////////////////////////////////////////////////////////

function runArangodRecovery (params, useEncryption, isKillAfterSetup = true) {
  let additionalParams= {
    'foxx.queues': 'false',
    'server.statistics': 'false',
    'log.foreground-tty': 'true',
    'database.ignore-datafile-errors': 'false', // intentionally false!
    'temp.path': params.temp_path
  };
  
  if (useEncryption) {
    // randomly turn on or off hardware-acceleration for encryption for both
    // setup and the actual test. given enough tests, this will ensure that we run
    // a good mix of accelerated and non-accelerated encryption code. in addition,
    // we shuffle between the setup and the test phase, so if there is any
    // incompatibility between the two modes, this will likely find it
    additionalParams['rocksdb.encryption-hardware-acceleration'] = (Math.random() * 100 >= 50) ? "true" : "false";
  }

  if (params.setup) {
    // special handling for crash-handler recovery tests
    if (params.script.match(/crash-handler/)) {
      // forcefully enable crash handler, even if turned off globally
      // during testing
      internal.env["ARANGODB_OVERRIDE_CRASH_HANDLER"] = "on";
    }

    params.options.disableMonitor = true;
    
    let instanceArgs = {};
    
    // enable development debugging if extremeVerbosity is set
    if (params.options.extremeVerbosity === true) {
      instanceArgs['log.level'] = 'development=info';
    }
    instanceArgs = Object.assign(instanceArgs, params.options.extraArgs);
    instanceArgs = Object.assign(instanceArgs, {
      'rocksdb.wal-file-timeout-initial': 10,
      'replication.auto-start': 'true',
    });
    if (!params.options.cluster) {
      instanceArgs = Object.assign(instanceArgs, {
        'log.output': 'file://' + params.crashLog,
        'log.level': 'INFO'
      });
    }
    if (useEncryption) {
      params.keyDir = fs.join(fs.getTempPath(), 'arango_encryption');
      if (!fs.exists(params.keyDir)) {
        fs.makeDirectory(params.keyDir);
      }

      const key = '01234567890123456789012345678901';
      
      let keyfile = fs.join(params.keyDir, 'rocksdb-encryption-keyfile');
      fs.write(keyfile, key);

      // special handling for encryption-keyfolder tests
      if (params.script.match(/encryption-keyfolder/)) {
        instanceArgs['rocksdb.encryption-keyfolder'] = params.keyDir;
        process.env["rocksdb-encryption-keyfolder"] = params.keyDir;
      } else {
        instanceArgs['rocksdb.encryption-keyfile'] = keyfile;
        process.env["rocksdb-encryption-keyfile"] = keyfile;
      }
    }

    params.instanceArgs = instanceArgs;
      
  }

  process.env["state-file"] = params.stateFile;
  if (!params.options.cluster) {
    process.env["crash-log"] = params.crashLog;
  }
  process.env["isSan"] = params.options.isSan;

  if (params.setup) {
    params.instanceArgs = Object.assign(params.instanceArgs, additionalParams);
    params.instanceManager = new im.instanceManager(params.options.protocol,
                                                    params.options,
                                                    params.instanceArgs,
                                                    fs.join('recovery',
                                                            params.count.toString()));
    params.instanceManager.prepareInstance();
    params.instanceManager.launchTcpDump("");
    params.instanceManager.nonfatalAssertSearch();
    try {
      if (!params.instanceManager.launchInstance()) {
        params.instanceManager.destructor(false);
        return {
          export: {
            status: false,
            message: 'failed to start server!'
          }
        };
      }
    } catch (ex) {
      params.options.cleanup = false;
      params.instanceManager.destructor(false);
      return {
        export: {
          status: false,
          message: `failed to start server: ${ex}\n${ex.stack}`
        }
      };
    }
  } else {
    print(BLUE + "Restarting " + RESET);
    params.instanceManager.reStartInstance();
  }
  params.instanceManager.reconnect();
  internal.db._useDatabase('_system');

  let testCode = fs.readFileSync(params.script);
  global.instanceManager = params.instanceManager;
  let testFunc;
  let success = -1;
  try {
    let content = `(function(){ let runSetup=${params.setup?"true":"false"};${testCode}
}())`; // DO NOT JOIN WITH THE LINE ABOVE -- because of content could contain '//' at the very EOF
    success = executeScript(content, true, params.script);

    if (params.setup && success !== 0) {
      if ( typeof success === 'object' && !Array.isArray(success) && success !== null) {
        print(`faulty test: setup phase did not terminate with integer: ${params.script}: \n ${JSON.stringify(success)}`);
        throw new Error(`faulty test: ${params.script} - setup phase did not terminate with integer`);
      }
      print('Nonzero exit code of setup: ' + JSON.stringify(success));
      params.instanceManager.shutdownInstance(false);
      params.instanceManager.destructor(false);
      return {
        status: false,
        message: 'Nonzero exit code of test: ' + JSON.stringify(success)
      };
    }
  } catch (ex) {
    let msg;
    if (ex instanceof ArangoError) {
     msg = `test '${params.script} Arango Error during test execution:\n${ex.errorNum}: ${ex.message}\n${ex.stack}`;
    } else {
      msg = `test '${params.script} failed by javascript error: \n${ex.stack}`;
    }
    print(`${RED}${msg}${RESET}`);
    if (!params.instanceManager.checkDebugTerminated()) {
      params.instanceManager.shutdownInstance(false);
    }
    params.instanceManager.destructor(false);
    return {
      status: false,
      message: `Error from: '${params.script}'\n${msg}`,
      stack: ex.stack
    };
  }
  if (params.setup) {
    let dbServers = params.instanceManager.arangods;
    if (params.options.cluster) {
      dbServers = dbServers.filter(
        (a) => {
          return a.isRole(inst.instanceRole.dbServer) ||
            a.isRole(inst.instanceRole.coordinator);
        });
    }

    if (isKillAfterSetup) {
      print(BLUE + "killing " + dbServers.length + " DBServers/Coordinators/Singles " + RESET);
      dbServers.forEach((arangod) => {
        params.instanceManager.debugTerminate();
      });
    }
    return {
      status: true,
      message: ""
    };
  } else {
    success['shutdown'] = {
      status:  params.instanceManager.shutdownInstance(false),
      message: "during shutdown"
    };
    return success;
  }
}

function _recovery (options, recoveryTests) {
  if (!versionHas('failure-tests') || !versionHas('maintainer-mode')) {
    return {
      recovery: {
        status: false,
        message: 'failure-tests not enabled. please recompile with -DUSE_FAILURE_TESTS=On and -DUSE_MAINTAINER_MODE=On'
      },
      status: false
    };
  }
  if (options.isInstrumented) {
    arango.timeout(arango.timeout() * 4);
  }
  let timeout = arango.timeout();
  let localOptions = _.clone(options);
  localOptions.enableAliveMonitor = false;

  let results = {
    status: true
  };
  let useEncryption = isEnterprise();

  recoveryTests = tu.splitBuckets(localOptions, recoveryTests);

  let count = 0;
  let tmpMgr = new tmpDirMmgr('recovery', localOptions);

  for (let i = 0; i < recoveryTests.length; ++i) {
    let test = recoveryTests[i];
    let filtered = {};
    arango.timeout(timeout);

    if (tu.filterTestcaseByOptions(test, localOptions, filtered)) {
      count += 1;
      ////////////////////////////////////////////////////////////////////////
      print(BLUE + "running setup of test " + count + " - " + test + RESET);
      let params = {
        tempDir: tmpMgr.tempDir,
        rootDir: fs.join(fs.getTempPath(), 'recovery', count.toString()),
        options: _.cloneDeep(localOptions),
        script: test,
        setup: true,
        count: count,
        keyDir: "",
        temp_path: fs.join(tmpMgr.tempDir, count.toString()),
        crashLogDir: fs.join(fs.getTempPath(), `crash_${count}`),
        crashLog: "",
      };
      fs.makeDirectoryRecursive(params.crashLogDir);
      params.crashLog = fs.join(params.crashLogDir, 'crash.log');
      fs.makeDirectoryRecursive(params.rootDir);
      fs.makeDirectoryRecursive(params.temp_path);
      let ret = runArangodRecovery(params, useEncryption, !doNotKillTests.includes(test));
      localOptions.cleanup &= params.options.cleanup;
      if (!ret.status) {
        results[test] = ret;
        results.status = false;
        continue;
      }
      ////////////////////////////////////////////////////////////////////////
      print(BLUE + "running recovery of test " + count + " - " + test + RESET);
      params.options.disableMonitor = localOptions.disableMonitor;
      params.setup = false;
      try {
        results[test] = runArangodRecovery(params, useEncryption);
        results.status = results.status && results[test].status;
      } catch (err) {
        results[test] = {
          failed: 1,
          status: false,
          message: `Crashed! \n${err.message}\nAborting execution of more tests\n${err.stack}`,
          duration: -1
        };
        results.status = false;
        results.crashed = true;
        print("skipping more tests!");
        return results;
      }

      params.instanceManager.destructor(results[test].status);
      if (results[test].status) {
        if (params.keyDir !== "") {
          fs.removeDirectoryRecursive(params.keyDir);
        }
      } else {
        print("Not cleaning up " + params.rootDir);
        results.status = false;
      }
    } else {
      if (localOptions.extremeVerbosity) {
        print('Skipped ' + test + ' because of ' + filtered.filter);
      }
    }
  }
  tmpMgr.destructor(localOptions.cleanup && results.status);
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

function recovery (options) {
  options.agency = undefined;
  options.cluster = false; 
  options.singles = 1;
  let recoveryTests = tu.scanTestPaths(testPaths.recovery, options);
  return _recovery(options, recoveryTests);
}

function recovery_cluster (options) {
  options.cluster = true;
  let recoveryTests = tu.scanTestPaths(testPaths.recovery_cluster, options);
  return _recovery(options, recoveryTests);
}

exports.setup = function (testFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['recovery'] = recovery;
  testFns['recovery_cluster'] = recovery_cluster;
  tu.CopyIntoObject(fnDocs, functionsDocumentation);
};
