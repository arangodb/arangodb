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
// //////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'recovery': 'run recovery tests',
  'recovery_cluster': 'run recovery cluster tests'
};

const fs = require('fs');
const internal = require('internal');
const tu = require('@arangodb/testutils/test-utils');
const cr = require('@arangodb/testutils/crash-recovery');
const tmpDirMmgr = require('@arangodb/testutils/tmpDirManager').tmpDirManager;
const { isEnterprise } = require("@arangodb/test-helper");

const BLUE = internal.COLORS.COLOR_BLUE;
const RESET = internal.COLORS.COLOR_RESET;

// At the moment only view-tests supported by cluster recovery tests:
const testPaths = {
  'recovery': [tu.pathForTesting('client/recovery')],
  'recovery_cluster': [tu.pathForTesting('client/recovery/cluster/crash'), tu.pathForTesting('client/recovery/cluster/search')],
};

// These tests should NOT be killed after the setup phase.
const doNotKillTests = [
  "tests/js/client/recovery/check-warnings-exitzero.js"
];

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: recovery
// //////////////////////////////////////////////////////////////////////////////

// returns the directory holding the key file, so it can be removed afterwards
function setupEncryption (script, instanceArgs) {
  const keyDir = fs.join(fs.getTempPath(), 'arango_encryption');
  if (!fs.exists(keyDir)) {
    fs.makeDirectory(keyDir);
  }

  const key = '01234567890123456789012345678901';

  let keyfile = fs.join(keyDir, 'rocksdb-encryption-keyfile');
  fs.write(keyfile, key);

  // special handling for encryption-keyfolder tests
  if (script.match(/encryption-keyfolder/)) {
    instanceArgs['rocksdb.encryption-keyfolder'] = keyDir;
    process.env["rocksdb-encryption-keyfolder"] = keyDir;
  } else {
    instanceArgs['rocksdb.encryption-keyfile'] = keyfile;
    process.env["rocksdb-encryption-keyfile"] = keyfile;
  }
  return keyDir;
}

function recoveryArgs (options, params) {
  let instanceArgs = {};

  // enable development debugging if extremeVerbosity is set
  if (options.extremeVerbosity === true) {
    instanceArgs['log.level'] = 'development=info';
  }
  instanceArgs = Object.assign(instanceArgs, options.extraArgs);
  instanceArgs['rocksdb.wal-file-timeout-initial'] = 10;
  if (!options.cluster) {
    instanceArgs['log.output'] = 'file://' + params.crashLog;
    instanceArgs['log.level'] = 'INFO';
  }
  if (params.useEncryption) {
    params.keyDir = setupEncryption(params.script, instanceArgs);
    // randomly turn on or off hardware-acceleration for encryption. given
    // enough tests, this will ensure that we run a good mix of accelerated
    // and non-accelerated encryption code.
    instanceArgs['rocksdb.encryption-hardware-acceleration'] = (Math.random() * 100 >= 50) ? "true" : "false";
  }
  return Object.assign(instanceArgs, {
    'log.foreground-tty': 'true',
    'database.ignore-datafile-errors': 'false', // intentionally false!
    'temp.path': params.tempPath
  });
}

// a test requests another round by writing to the state file
function wantsAnotherRound (stateFile) {
  try {
    return String(fs.readFileSync(stateFile)).length > 0;
  } catch (err) {
    return false;
  }
}

function runRecoveryTest (options, test, count, tmpMgr, useEncryption) {
  const stateFile = fs.getTempFile();
  // special handling for crash-handler recovery tests
  if (test.match(/crash-handler/)) {
    // forcefully enable crash handler, even if turned off globally
    // during testing
    internal.env["ARANGODB_OVERRIDE_CRASH_HANDLER"] = "on";
  }
  let iteration = 0;
  while (true) {
    ++iteration;
    const name = `${count}_${iteration}`;
    print(BLUE + "running round #" + iteration + " of test " + count + " - " + test + RESET);
    let params = {
      script: test,
      useEncryption,
      keyDir: "",
      tempPath: fs.join(tmpMgr.tempDir, name),
      crashLogDir: fs.join(fs.getTempPath(), `crash_${name}`),
    };
    params.crashLog = fs.join(params.crashLogDir, 'crash.log');
    fs.makeDirectoryRecursive(params.crashLogDir);
    fs.makeDirectoryRecursive(params.tempPath);

    process.env["state-file"] = stateFile;
    if (!options.cluster) {
      process.env["crash-log"] = params.crashLog;
    }
    process.env["isSan"] = options.isSan;

    const result = cr.runCrashRecoveryCycle(
      options, test, recoveryArgs(options, params), fs.join('recovery', name),
      { killAfterSetup: !doNotKillTests.includes(test) });

    if (!result.status) {
      print("Not cleaning up " + params.crashLogDir);
      return result;
    }
    if (options.cleanup) {
      fs.removeDirectoryRecursive(params.crashLogDir, true);
      if (params.keyDir !== "") {
        fs.removeDirectoryRecursive(params.keyDir, true);
      }
    }
    if (!wantsAnotherRound(stateFile)) {
      return result;
    }
    print('Going into next iteration of recovery_server test');
  }
}

function _recovery (options, recoveryTests) {
  const unsupported = cr.missingBuildSupport('recovery');
  if (unsupported !== null) {
    return unsupported;
  }
  if (options.isInstrumented) {
    arango.timeout(arango.timeout() * 4);
  }
  const timeout = arango.timeout();
  const useEncryption = isEnterprise();
  let tmpMgr = new tmpDirMmgr('recovery', options);

  const results = cr.runCrashRecoveryTests(options, recoveryTests, (localOptions, test, count) => {
    arango.timeout(timeout);
    return runRecoveryTest(localOptions, test, count, tmpMgr, useEncryption);
  });
  tmpMgr.destructor(options.cleanup && results.status);
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
