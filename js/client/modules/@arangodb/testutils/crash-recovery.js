/* jshint strict: false, sub: true */
/* global print */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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

// Shared driver for test suites whose test files run twice against the same
// data: once with `runSetup === true`, after which the SUT is crashed, and
// once with `runSetup === false` after the SUT was restarted.

const fs = require('fs');
const internal = require('internal');
const _ = require('lodash');
const tu = require('@arangodb/testutils/test-utils');
const im = require('@arangodb/testutils/instance-manager');
const { versionHas } = require('@arangodb/test-helper');
const ArangoError = require('@arangodb').ArangoError;

const RED = internal.COLORS.COLOR_RED;
const RESET = internal.COLORS.COLOR_RESET;
const BLUE = internal.COLORS.COLOR_BLUE;

// crashing via REST is only available in maintainer builds
function missingBuildSupport (suiteName) {
  if (versionHas('failure-tests') && versionHas('maintainer-mode')) {
    return null;
  }
  return {
    [suiteName]: {
      status: false,
      message: 'failure-tests not enabled. please recompile with -DUSE_FAILURE_TESTS=On and -DUSE_MAINTAINER_MODE=On'
    },
    status: false
  };
}

function executeTestScript (script, { runSetup = false, getOptions = false }) {
  const content = `(function(){ const runSetup = ${runSetup}; const getOptions = ${getOptions};${fs.readFileSync(script)}
}())`; // DO NOT JOIN WITH THE LINE ABOVE -- content could end in a '//' comment
  return internal.executeScript(content, true, script);
}

function errorResult (script, ex) {
  const msg = (ex instanceof ArangoError)
        ? `test '${script}' Arango Error during test execution:\n${ex.errorNum}: ${ex.message}\n${ex.stack}`
        : `test '${script}' failed by javascript error:\n${ex.stack}`;
  print(`${RED}${msg}${RESET}`);
  return { status: false, message: msg };
}

function runTestPhase (instanceManager, script, runSetup) {
  instanceManager.reconnect();
  internal.db._useDatabase('_system');
  global.instanceManager = instanceManager;
  return executeTestScript(script, { runSetup });
}

function stopAfterFailure (instanceManager) {
  if (!instanceManager.checkDebugTerminated()) {
    instanceManager.shutdownInstance(false);
  }
  instanceManager.destructor(false);
}

// A test file may declare its own startup arguments by returning them when
// `getOptions === true`; they apply to the first launch and to the restart.
// Files not mentioning it must not be run, as they would execute their checks.
function testStartupArgs (script) {
  if (String(fs.readFileSync(script)).search('getOptions') < 0) {
    return {};
  }
  return executeTestScript(script, { getOptions: true });
}

// Exceptions from restarting the crashed SUT propagate: the SUT is in an
// unknown state, and runCrashRecoveryTests aborts the whole suite.
function runCrashRecoveryCycle (options, script, instanceArgs, name, { killAfterSetup = true } = {}) {
  // the instanceManager writes into its args, which must not leak between tests
  const args = Object.assign({}, instanceArgs, testStartupArgs(script));
  const instanceManager = new im.instanceManager(options.protocol, options, args, name);
  instanceManager.prepareInstance();
  instanceManager.launchTcpDump('');
  // the crash after setup is intentional and must not fail the run
  instanceManager.nonfatalAssertSearch();
  try {
    if (!instanceManager.launchInstance()) {
      instanceManager.destructor(false);
      return { status: false, message: 'failed to start server!' };
    }
  } catch (ex) {
    instanceManager.destructor(false);
    return { status: false, message: `failed to start server: ${ex}\n${ex.stack}` };
  }

  try {
    const rc = runTestPhase(instanceManager, script, true);
    if (rc !== 0) {
      throw new Error(`faulty test: setup phase returned ${JSON.stringify(rc)} instead of 0`);
    }
  } catch (ex) {
    stopAfterFailure(instanceManager);
    return errorResult(script, ex);
  }
  if (killAfterSetup) {
    instanceManager.debugTerminate('crashing SUT after setup');
  }

  internal.SetGlobalExecutionDeadlineTo(options.oneTestTimeout / 4);
  print(BLUE + 'Restarting ' + RESET);
  instanceManager.reStartInstance();
  let result;
  try {
    result = runTestPhase(instanceManager, script, false);
  } catch (ex) {
    stopAfterFailure(instanceManager);
    return errorResult(script, ex);
  }
  result.shutdown = {
    status: instanceManager.shutdownInstance(false),
    message: 'during shutdown'
  };
  instanceManager.destructor(result.status);
  return result;
}

// runTest(options, test, count) runs one test file and returns its result
function runCrashRecoveryTests (options, tests, runTest) {
  let localOptions = _.clone(options);
  // the SUT is crashed on purpose, the alive monitor must not flag that
  localOptions.enableAliveMonitor = false;

  let results = { status: true };
  let count = 0;
  for (const test of tu.splitBuckets(localOptions, tests)) {
    let filtered = {};
    if (!tu.filterTestcaseByOptions(test, localOptions, filtered)) {
      if (localOptions.extremeVerbosity) {
        print('Skipped ' + test + ' because of ' + filtered.filter);
      }
      continue;
    }
    count += 1;
    print(BLUE + 'running test ' + count + ' - ' + test + RESET);
    try {
      results[test] = runTest(localOptions, test, count);
    } catch (err) {
      results[test] = {
        failed: 1,
        status: false,
        message: `Crashed! \n${err.message}\nAborting execution of more tests\n${err.stack}`,
        duration: -1
      };
      results.status = false;
      results.crashed = true;
      print('skipping more tests!');
      return results;
    }
    results.status = results.status && results[test].status;
  }

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

exports.missingBuildSupport = missingBuildSupport;
exports.executeTestScript = executeTestScript;
exports.runCrashRecoveryCycle = runCrashRecoveryCycle;
exports.runCrashRecoveryTests = runCrashRecoveryTests;
