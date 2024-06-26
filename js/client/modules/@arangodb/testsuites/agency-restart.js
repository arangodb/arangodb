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
  'agency-restart': 'run recovery tests'
};

const fs = require('fs');
const pu = require('@arangodb/testutils/process-utils');
const tu = require('@arangodb/testutils/test-utils');
const trs = require('@arangodb/testutils/testrunners');
const inst = require('@arangodb/testutils/instance');
const {agencyMgr} = require('@arangodb/testutils/agency');
const _ = require('lodash');
const tmpDirMmgr = require('@arangodb/testutils/tmpDirManager').tmpDirManager;

const toArgv = require('internal').toArgv;
const versionHas = require("@arangodb/test-helper").versionHas;

const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
const BLUE = require('internal').COLORS.COLOR_BLUE;

const testPaths = {
  'agency-restart': [tu.pathForTesting('client/agency-restart')]
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: agency_restart
// //////////////////////////////////////////////////////////////////////////////

function runArangodRecovery (params, agencyMgr) {
  let additionalParams= {
    'javascript.enabled': 'true',
    'agency.activate': 'true',
    'agency.compaction-keep-size': '10000',
    'agency.wait-for-sync': 'false',
    'agency.size': '1',
  };

  let argv = [];
  let binary = pu.ARANGOD_BIN;

  if (params.setup) {
    additionalParams['javascript.script-parameter'] = 'setup';

    params.options.disableMonitor = true;
    params.testDir = fs.join(params.tempDir, `${params.count}`);

    // enable development debugging if extremeVerbosity is set
    let args = Object.assign({
      'server.rest-server': 'false',
      'javascript.script': params.script,
      'log.output': 'file://' + params.crashLog
    }, params.options.extraArgs);

    if (params.options.extremeVerbosity === true) {
      args['log.level'] = 'development=info';
    }
    params['instance'] = new inst.instance(params.options,
                                           inst.instanceRole.agent,
                                           args, {}, 'tcp', params.rootDir, '',
                                           agencyMgr);

    argv = toArgv(Object.assign(params.instance.args, additionalParams));
  } else {
    additionalParams['javascript.script-parameter'] = 'recovery';
    argv = toArgv(Object.assign(params.instance.args, additionalParams));

    if (params.options.rr) {
      binary = 'rr';
      argv.unshift(pu.ARANGOD_BIN);
    }
  }

  process.env["crash-log"] = params.crashLog;
  params.instance.pid = pu.executeAndWait(
    binary,
    argv,
    params.options,
    'recovery',
    params.instance.rootDir,
    !params.setup && params.options.coreCheck);
}

function agencyRestart (options) {
  if (!versionHas('failure-tests')) {
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
  let tmpMgr = new tmpDirMmgr('agency-restart', options);

  for (let i = 0; i < agencyRestartTests.length; ++i) {
    let test = agencyRestartTests[i];
    let filtered = {};

    if (tu.filterTestcaseByOptions(test, options, filtered)) {
      count += 1;
      ////////////////////////////////////////////////////////////////////////
      print(BLUE + "running setup of test " + count + " - " + test + RESET);
      let params = {
        tempDir: tmpMgr.tempDir,
        rootDir: fs.join(fs.getTempPath(), 'agency-restart', count.toString()),
        options: _.cloneDeep(options),
        script: test,
        setup: true,
        count: count,
        testDir: "",
        crashLogDir: fs.join(fs.getTempPath(), `crash_${count}`),
        crashLog: ""
      };
      fs.makeDirectoryRecursive(params.crashLogDir);
      params.crashLog = fs.join(params.crashLogDir, 'crash.log');
      let agencyMgrInst = new agencyMgr(options, null);
      runArangodRecovery(params, agencyMgrInst);

      ////////////////////////////////////////////////////////////////////////
      print(BLUE + "running recovery of test " + count + " - " + test + RESET);
      params.options.disableMonitor = options.disableMonitor;
      params.setup = false;
      try {
        trs.writeTestResult(params.instance.args['temp.path'], {
          failed: 1,
          status: false,
          message: "unable to run agency_restart test " + test,
          duration: -1
        });
      } catch (er) {}
      runArangodRecovery(params, agencyMgrInst);

      results[test] = trs.readTestResult(
        params.instance.args['temp.path'],
        {
          status: false
        },
        test
      );
      if (results[test].status && options.cleanup) {
        fs.removeDirectoryRecursive(params.testDir, true);
        fs.removeDirectoryRecursive(params.crashLogDir, true);
      } else {
        print("Not cleaning up " + params.testDir);
        results.status &= results[test].status;
      }
    } else {
      if (options.extremeVerbosity) {
        print('Skipped ' + test + ' because of ' + filtered.filter);
      }
    }
  }
  tmpMgr.destructor(results.status);
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
  testFns['agency-restart'] = agencyRestart;
  tu.CopyIntoObject(fnDocs, functionsDocumentation);
};
