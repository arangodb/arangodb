/* jshint strict: false, sub: true */
/* global print db */
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
// / @author Wilfried Goesgens
// //////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'rta_makedata': 'Release Testautomation Makedata / Checkdata framework'
};

const internal = require('internal');

const executeExternal = internal.executeExternal;
const executeExternalAndWait = internal.executeExternalAndWait;
const statusExternal = internal.statusExternal;

/* Modules: */
const _ = require('lodash');
const fs = require('fs');
const pu = require('@arangodb/testutils/process-utils');
const ct = require('@arangodb/testutils/client-tools');
const tu = require('@arangodb/testutils/test-utils');
const im = require('@arangodb/testutils/instance-manager');
const inst = require('@arangodb/testutils/instance');
const SetGlobalExecutionDeadlineTo = require('internal').SetGlobalExecutionDeadlineTo;
const testRunnerBase = require('@arangodb/testutils/testrunner').testRunner;
const yaml = require('js-yaml');
const platform = require('internal').platform;
const time = require('internal').time;
const isEnterprise = require("@arangodb/test-helper").isEnterprise;

// const BLUE = require('internal').COLORS.COLOR_BLUE;
// const CYAN = require('internal').COLORS.COLOR_CYAN;
const GREEN = require('internal').COLORS.COLOR_GREEN;
const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

const testPaths = {
  'rta_makedata': []
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: shell_http
// //////////////////////////////////////////////////////////////////////////////

function makeDataWrapper (options) {
  let stoppedDbServerInstance = {};
  if (options.hasOwnProperty('test') && (typeof (options.test) !== 'undefined')) {
    if (!options.hasOwnProperty('makedata_args')) {
      options['makedata_args'] = {};
    }
    options['makedata_args']['test'] = options.test;
  }

  class rtaMakedataRunner extends testRunnerBase {
    constructor(options, testname, ...optionalArgs) {
      super(options, testname, ...optionalArgs);
      this.info = "runRtaInArangosh";
      if (isEnterprise()) {
        this.serverOptions["arangosearch.columns-cache-limit"] = "5000";
      }
      this.continueTesting = true;
    }
    filter(te, filtered) {
      return true;
    }
    
    checkSutCleannessBefore(te) {
      if (this.continueTesting) {
        return super.checkSutCleannessBefore(te);
      }
      return false;
    }
    checkSutCleannessAfter(te) {
      if (this.continueTesting) {
        return super.checkSutCleannessAfter(te);
      }
      return false;
    }
    runOneTest(file) {
      if (!this.continueTesting) {
        return {
          'forceTerminate': true,
          'message': `skipped due to previous failure`,
          'failed': 1,
          'status': false,
          'duration': 0.0
        };
      }
      let res = {'total':0, 'duration':0.0, 'status':true, message: '', 'failed': 0};
      let messages = [
        "initially create the test data",
        "revalidate the test data for the first time",
        "obstruct system, and check whether the data is still valid",
        "cleaning up test data generated by makedata"
      ];
      let count = 0;
      let counters = { nonAgenciesCount: 1};
      [
        0, // makedata
        1, // checkdata
        1, // checkdata (with resillience test - instances stopped)
        2  // clear data
      ].forEach(testCount => {
        let moreargv = [];
        count += 1;
        if (this.options.cluster) {
          if (count === 2) {
            let rc = ct.run.rtaWaitShardsInSync(this.options, this.instanceManager);
            if (rc.status) {
              this.continueTesting = false;
              return {
                'forceTerminate': true,
                'message': `shards would not get in sync ${rc}`,
                'failed': 1,
                'status': false,
                'duration': 0.0
              };
            }
          }
          if (count === 2) {
            try {
              this.instanceManager.upgradeCycleInstance();
            } catch(e) {
              return {
                'forceTerminate': true,
                'message': `upgradeCycle failed by: ${e.message}\n${e.stack}`,
                'failed': 1,
                'status': false,
                'duration': 0.0
              };
            }
          }
          if (count === 3) {
            this.instanceManager.arangods.forEach(function (oneInstance, i) {
              if (oneInstance.isRole(inst.instanceRole.dbServer)) {
                stoppedDbServerInstance = oneInstance;
              }
            });
            print('stopping dbserver ' + stoppedDbServerInstance.name +
                  ' ID: ' + stoppedDbServerInstance.id +JSON.stringify( stoppedDbServerInstance.getStructure()));
            try {
              this.instanceManager.resignLeaderShip(stoppedDbServerInstance);
            } catch(e) {
              this.continueTesting = false;
              return {
                'forceTerminate': true,
                'message': `resigning leadership failed by: ${e.message}\n${e.stack}`,
                'failed': 1,
                'status': false,
                'duration': 0.0
              };
            }
            stoppedDbServerInstance.shutDownOneInstance(counters, false, 10);
            stoppedDbServerInstance.waitForExit();
            moreargv = [ '--disabledDbserverUUID', stoppedDbServerInstance.id];
            if (this.options.replicationVersion === 2 || this.options.replicationVersion === "2") {
              this.instanceManager.removeServerFromAgency(stoppedDbServerInstance.id);
            }
          }
        }
        let logFile = fs.join(fs.getTempPath(), `rta_out_${count}.log`);
        require('internal').env.INSTANCEINFO = JSON.stringify(this.instanceManager.getStructure());
        let rc = ct.run.rtaMakedata(this.options, this.instanceManager, testCount, messages[count-1], logFile, moreargv);
        if (!rc.status) {
          this.continueTesting = false;
          let rx = new RegExp(/\\n/g);
          res.message += file + ':\n' + fs.read(logFile).replace(rx, '\n');
          res.status = false;
          res.failed += 1;
        } else {
          fs.remove(logFile);
        }
        res.total++;
        res.duration += rc.duration;
        if ((this.options.cluster) && (count === 3)) {
          print('relaunching dbserver');
          stoppedDbServerInstance.restartOneInstance({});
        }
      });
      return res;
    }
  }
  let localOptions = Object.assign({}, options, tu.testServerAuthInfo);
  if (localOptions.cluster && localOptions.dbServers < 3) {
    localOptions.dbServers = 3;
  }

  SetGlobalExecutionDeadlineTo(localOptions.oneTestTimeout * 1000);
  let rc = new rtaMakedataRunner(localOptions, 'rta_makedata_test').run(['rta']);
  let timeout = SetGlobalExecutionDeadlineTo(0.0);
  if (timeout) {
    return {
      timeout: true,
      forceTerminate: true,
      status: false,
      message: `test aborted due to >>${require('internal').getDeadlineReasonString()}<<. Original test status: ${JSON.stringify(rc)}`,
    };
  }
  options.cleanup = options.cleanup && localOptions.cleanup;
  return rc;
}


exports.setup = function (testFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['rta_makedata'] = makeDataWrapper;
  tu.CopyIntoObject(fnDocs, {
    'rta_makedata': 'Release Testautomation Makedata / Checkdata framework'
  });
};
