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
const replication = require("@arangodb/replication");
const compareTicks = replication.compareTicks;
const SetGlobalExecutionDeadlineTo = require('internal').SetGlobalExecutionDeadlineTo;
const testRunnerBase = require('@arangodb/testutils/testrunner').testRunner;
const yaml = require('js-yaml');
const platform = require('internal').platform;
const time = require('internal').time;
const isEnterprise = require("@arangodb/test-helper").isEnterprise;

// const BLUE = require('internal').COLORS.COLOR_BLUE;
const CYAN = require('internal').COLORS.COLOR_CYAN;
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
      if (!this.options.cluster) {
        this.options.singles = 2;
        this.addArgs = {};
      }
      this.info = "runRtaInArangosh";
      if (isEnterprise()) {
        this.serverOptions["arangosearch.columns-cache-limit"] = "5000";
      }
      this.continueTesting = true;
    }
    filter(te, filtered) {
      return true;
    }

    waitForReplState() {
      print(`${CYAN}${Date()} waiting for follower to catch up!${RESET}`);
      let state = {};
      var printed = false;
      state.lastLogTick = replication.logger.state().state.lastUncommittedLogTick;

      this.instanceManager.arangods[1].toThisInstance(() => {
        while (true) {
          let followerState = replication.globalApplier.state();

          if (followerState.state.lastError.errorNum > 0) {
            print("follower has errored:", JSON.stringify(followerState.state.lastError));
            throw new Error(JSON.stringify(followerState.state.lastError));
          }

          if (!followerState.state.running) {
            break;
          }

          if (compareTicks(followerState.state.lastAppliedContinuousTick, state.lastLogTick) >= 0 ||
              compareTicks(followerState.state.lastProcessedContinuousTick, state.lastLogTick) >= 0) { // ||
            print("follower has caught up. state.lastLogTick:", state.lastLogTick, "followerState.lastAppliedContinuousTick:", followerState.state.lastAppliedContinuousTick, "followerState.lastProcessedContinuousTick:", followerState.state.lastProcessedContinuousTick);
            break;
          }

          if (!printed) {
            print("waiting for follower to catch up");
            printed = true;
          }
          internal.wait(0.5, false);
        }
      });
    }    
    preStart() {
      if (!this.options.cluster) {
        // our tests lean on accessing the `_users` collection, hence no auth for secondary
        this.instanceManager.arangods[1].args['server.authentication'] = false;
      }
      return {
        message: '',
        state: true,
      };
    }
    postStart() {
      if (!this.options.cluster) {
        let message;
        print("starting replication follower: ");
        let state = true;
        this.addArgs['flatCommands'] = [this.instanceManager.arangods[1].endpoint];
        [0, 1].forEach(which => {
          this.instanceManager.endpoint = this.instanceManager.arangods[which].endpoint;
          this.instanceManager.arangods[which].toThisInstance(() => {
            try {
              var users = require("@arangodb/users");
              users.save("replicator-user", "replicator-password", true);
              users.grantDatabase("replicator-user", "_system");
              users.grantCollection("replicator-user", "_system", "*", "rw");
              users.reload();
            } catch(ex) {
              state = false;
              message += `failed to connect ${this.instanceManager.arangods[which].name}: ex.message\n`;
              print(RED + message + RESET);
            }
          });
        });
        let syncResult;
        this.instanceManager.arangods[1].toThisInstance(() => {
          syncResult = replication.sync({
            endpoint: this.instanceManager.arangods[0].endpoint,
            username: "root",
            password: "",
            verbose: true,
            includeSystem: false,
            keepBarrier: true,
          });
          if (!syncResult.hasOwnProperty('lastLogTick')) {
            throw new Error(`sync result doesn't have a lostLogTick: ${JSON.stringify(syncResult)}`);
          }
          // use lastLogTick as of now
          state = { lastLogTick: replication.logger.state().state.lastLogTick};

          let applierConfiguration = {
            endpoint: this.instanceManager.arangods[0].endpoint,
            username: "root",
            password: "", 
            requireFromPresent: true 
          };

          replication.applier.properties(applierConfiguration);
          replication.applier.start(syncResult.lastLogTick, syncResult.barrierId);
        });
        this.waitForReplState();
        return {
          message: message,
          state: state,
        };
      } else {
        return {
          message: '',
          state: true,
        };
      }
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
      this.options.rtaNegFilter = "";
      if (this.options.skipServerJS) {
        // TODO: QA-703
        this.options.rtaNegFilter = "070,071,801,550,900,960";
      }
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
        let whichRTA = `rta_${count}`;
        if (this.options.cluster) {
          if (count === 2) {
            let rc = ct.run.rtaWaitShardsInSync(this.options, this.instanceManager);
            if (!rc.status) {
              this.continueTesting = false;
              res.status = false;
              res.failed += 1;
              res[whichRTA] = {
                'forceTerminate': true,
                'message': `shards would not get in sync ${rc}`,
                'failed': 1,
                'status': false,
                'duration': 0.0
              };
              return;
            }
          }
          if (count === 2) {
            try {
              if (this.options.oldSource !== undefined) {
                print("switching binary set");
                pu.switchBinarySet(1);
              }
              this.instanceManager.upgradeCycleInstance();
            } catch(e) {
              res.status = false;
              res.failed += 1;
              res[whichRTA] = {
                'forceTerminate': true,
                'message': `upgradeCycle failed by: ${e.message}\n${e.stack}`,
                'failed': 1,
                'status': false,
                'duration': 0.0
              };
              return;
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
              res.status = false;
              res.failed += 1;
              res[whichRTA] = {
                'forceTerminate': true,
                'message': `resigning leadership failed by: ${e.message}\n${e.stack}`,
                'failed': 1,
                'status': false,
                'duration': 0.0
              };
              return;
            }
            stoppedDbServerInstance.shutDownOneInstance(counters, false, 10);
            stoppedDbServerInstance.waitForExit();
            moreargv = [ '--disabledDbserverUUID', stoppedDbServerInstance.id];
            if (this.options.replicationVersion === 2 || this.options.replicationVersion === "2") {
              this.instanceManager.removeServerFromAgency(stoppedDbServerInstance.id);
            }
          }
        } else {
          this.waitForReplState();
          if (count === 2) {
            try {
              if (this.options.oldSource !== undefined) {
                print("switching binary set");
                pu.switchBinarySet(1);
              }
              this.instanceManager.upgradeCycleInstance();
            } catch(e) {
              res.status = false;
              res.failed += 1;
              res[whichRTA] = {
                'forceTerminate': true,
                'message': `upgradeCycle failed by: ${e.message}\n${e.stack}`,
                'failed': 1,
                'status': false,
                'duration': 0.0
              };
              return;
            }
          }
        }
        let logFile = fs.join(fs.getTempPath(), `rta_out_${count}.log`);
        require('internal').env.INSTANCEINFO = JSON.stringify(this.instanceManager.getStructure());
        let rc = ct.run.rtaMakedata(this.options, this.instanceManager, testCount, messages[count-1], logFile, moreargv);
        res[whichRTA] = rc;
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
  localOptions.extraArgs['vector-index'] = true;

  SetGlobalExecutionDeadlineTo(localOptions.oneTestTimeout);
  let rc = new rtaMakedataRunner(localOptions, 'rta_makedata_test').run(['rta']);
  let timeout = SetGlobalExecutionDeadlineTo(0.0);
  options.cleanup = options.cleanup && localOptions.cleanup && rc.status;
  if (timeout) {
    return {
      failed: 1,
      timeout: true,
      forceTerminate: true,
      status: false,
      message: `test aborted due to >>${require('internal').getDeadlineReasonString()}<<. Original test status: ${JSON.stringify(rc)}`,
    };
  }
  return rc;
}


exports.setup = function (testFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['rta_makedata'] = makeDataWrapper;
  tu.CopyIntoObject(fnDocs, {
    'rta_makedata': 'Release Testautomation Makedata / Checkdata framework'
  });
};
