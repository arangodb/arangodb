/* jshint strict: false, sub: true */
/* global print db arango */
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
// / @author Wilfried Goesgens
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief runs a list of tests
// //////////////////////////////////////////////////////////////////////////////
const _ = require('lodash');
const fs = require('fs');
const pu = require('@arangodb/testutils/process-utils');
const tu = require('@arangodb/testutils/test-utils');
const time = require('internal').time;
const sleep = require('internal').sleep;
const userManager = require("@arangodb/users");

const GREEN = require('internal').COLORS.COLOR_GREEN;
const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
const YELLOW = require('internal').COLORS.COLOR_YELLOW;

let didSplitBuckets = false;


class testRunner {
  constructor(options, testname, serverOptions, checkUsers=true) {
    if (options.testBuckets && !didSplitBuckets) {
      throw new Error("You parametrized to split buckets, but this testsuite doesn't support it!!!");
    }
    this.addArgs = undefined;
    this.options = options;
    this.friendlyName = testname;
    this.serverOptions = serverOptions;
    if (this.serverOptions === undefined) {
      this.serverOptions = {};
    }
    this.testList = [];
    this.customInstanceInfos = {};
    this.memProfCounter = 0;
    this.env = {};
    this.results = {};
    this.continueTesting = true;
    this.usersCount = 0;
    this.cleanupChecks = [ ];
    let usersTests = {
      name: 'users',
      setUp: function (te) {
        if (testname !== 'agency' && testname !== 'authentication') {
          this.usersCount = userManager.all().length;
        }
        return true;
      },
      runCheck: function (te) {
        if ((testname !== 'agency') &&
            (testname !== 'authentication') &&
            (userManager.all().length !== this.usersCount)) {
          this.continueTesting = false;
          this.results[te] = {
            status: false,
            message: 'Cleanup of users missing - found users left over: [ ' +
              JSON.stringify(userManager.all()) +
              ' ] - Original test status: ' +
              JSON.stringify(this.results[te])
          };
          return false;
        }
        return true;
      }
    };
    if (checkUsers) {
      this.cleanupChecks.push(usersTests);
    }
    this.cleanupChecks.push({
      name: 'databases',
      setUp: function(obj, te){ return true;},
      runCheck: function(obj, te) {
        // TODO: we are currently filtering out the UnitTestDB here because it is 
        // created and not cleaned up by a lot of the `authentication` tests. This
        // should be fixed eventually
        let databasesAfter = db._databases().filter((name) => name !== 'UnitTestDB');
        if (databasesAfter.length !== 1 || databasesAfter[0] !== '_system') {
          this.results[te] = {
            status: false,
            message: 'Cleanup missing - test left over databases: ' + JSON.stringify(databasesAfter) + '. Original test status: ' + JSON.stringify(obj.results[te])
          };
          obj.continueTesting = false;
          return false;
        }
        return true;
      }
    });
    this.collectionsBefore = [];
    this.cleanupChecks.push({
      name: 'collections',
      setUp: function(obj, te) {
        try {
          db._collections().forEach(collection => {
            obj.collectionsBefore.push(collection._name);
          });
        } catch (x) {
          obj.results[te] = {
            status: false,
            message: 'failed to fetch the previously available collections: ' + x.message
          };
          obj.continueTesting = false;
          obj.serverDead = true;
          return false;
        }
        return true;
      },
      runCheck: function(obj, te) {
        let collectionsAfter = [];
        try {
          db._collections().forEach(collection => {
            collectionsAfter.push(collection._name);
          });
        } catch (x) {
          obj.results[te] = {
            status: false,
            message: 'failed to fetch the currently available collections: ' + x.message + '. Original test status: ' + JSON.stringify(obj.results[te])
          };
          obj.continueTesting = false;
          return false;
        }
        let delta = tu.diffArray(obj.collectionsBefore, collectionsAfter).filter(function(name) {
          return ! ((name[0] === '_') || (name === "compact") || (name === "election")
                    || (name === "log")); // exclude system/agency collections from the comparison
          return (name[0] !== '_'); // exclude system collections from the comparison
        });
        if (delta.length !== 0) {
          obj.results[te] = {
            status: false,
            message: 'Cleanup missing - test left over collection:' + delta + '. Original test status: ' + JSON.stringify(obj.results[te])
          };
          return false;
        }
        return true;
      }
    });
    this.viewsBefore = [];
    this.cleanupChecks.push({
      name: 'views',
      setUp: function(obj, te) {
        try {
          db._views().forEach(view => {
            obj.viewsBefore.push(view._name);
          });
        } catch (x) {
          this.results[te] = {
            status: false,
            message: 'failed to fetch the previously available views: ' + x.message
          };
          obj.continueTesting = false;
          obj.serverDead = true;
          return false;
        }
        return true;
      },
      runCheck: function(obj, te) {
        let viewsAfter = [];
        try {
          db._views().forEach(view => {
            viewsAfter.push(view._name);
          });
        } catch (x) {
          obj.results[te] = {
            status: false,
            message: 'failed to fetch the currently available views: ' + x.message + '. Original test status: ' + JSON.stringify(obj.results[te])
          };
          obj.continueTesting = false;
          return false;
        }
        let delta = tu.diffArray(obj.viewsBefore, viewsAfter).filter(function(name) {
          return ! ((name[0] === '_') || (name === "compact") || (name === "election")
                    || (name === "log")); // exclude system/agency collections from the comparison
        });
        if (delta.length !== 0) {
          obj.results[te] = {
            status: false,
            message: 'Cleanup missing - test left over view:' + delta + '. Original test status: ' + JSON.stringify(obj.results[te])
          };
          return false;
        }
        return true;
      }
    });

    this.graphCount = 0;
    this.cleanupChecks.push({
      name: 'graphs',
      setUp: function(obj, te) {
        this.graphCount = db._collection('_graphs').count();
        return true;
      },
      runCheck: function(obj, te) {
        let graphs;
        try {
          graphs = db._collection('_graphs');
        } catch (x) {
          obj.results[te] = {
            status: false,
            message: 'failed to fetch the graphs: ' + x.message + '. Original test status: ' + JSON.stringify(this.results[te])
          };
          obj.continueTesting = false;
          return false;
        }
        if (graphs && graphs.count() !== obj.graphCount) {
          obj.results[te] = {
            status: false,
            message: 'Cleanup of graphs missing - found graph definitions: [ ' +
              JSON.stringify(graphs.toArray()) +
              ' ] - Original test status: ' +
              JSON.stringify(obj.results[te])
          };
          obj.graphCount = graphs.count();
          return false;
        }
        return true;
      }
    });

    this.cleanupChecks.push({
      name: 'failurepoints',
      setUp: function(obj, te) { return true; },
      runCheck: function(obj, te) {
        let failurePoints = pu.checkServerFailurePoints(obj.instanceInfo);
        if (failurePoints.length > 0) {
          obj.continueTesting = false;
          obj.results[te] = {
            status: false,
            message: 'Cleanup of failure points missing - found failure points engaged: [ ' +
              JSON.stringify(failurePoints) +
              ' ] - Original test status: ' +
              JSON.stringify(obj.results[te])
          };
          return false;
        }
      }
    });

  }
  preStart() { return {state: true}; } // overload me
  startFailed() { return {state: true}; } // overload me
  postStart() { return {state: true}; } // overload me
  preRun() { return {state: true}; } // overload me
  preStop() { return {state: true}; } // overload me
  postStop() { return {state: true}; } // overload me

  alive() { return true; }
  healthCheck() {
    return pu.arangod.check.instanceAlive(this.instanceInfo, this.options) &&
      this.alive();
  }

  abortTestOnError(te) {
    if (!this.results.hasOwnProperty('SKIPPED')) {
      print('oops! Skipping remaining tests, server is unavailable for testing.');
      let originalMessage;
      if (this.results.hasOwnProperty(te) && this.results[te].hasOwnProperty('message')) {
        originalMessage = this.results[te].message;
      }
      this.results['SKIPPED'] = {
        status: false,
        message: ""
      };
      this.results[te] = {
        status: false,
        message: 'server unavailable for testing. ' + originalMessage
      };
    } else {
      if (this.results['SKIPPED'].message !== '') {
        this.results['SKIPPED'].message += ', ';
      }
      this.results['SKIPPED'].message += te;
    }

    this.instanceInfo.exitStatus = 'server is gone.';

  }

  loadClusterTestStabilityInfo(){
    try {
      if (this.instanceInfo.hasOwnProperty('clusterHealthMonitorFile')) {
        let status = true;
        let slow = [];
        let buf = fs.readBuffer(this.instanceInfo.clusterHealthMonitorFile);
        let lineStart = 0;
        let maxBuffer = buf.length;

        for (let j = 0; j < maxBuffer; j++) {
          if (buf[j] === 10) { // \n
            const line = buf.asciiSlice(lineStart, j);
            lineStart = j + 1;
            let val = JSON.parse(line);
            if (val.state === false) {
              slow.push(val);
              status = false;
            }
          }
        }
        if (!status) {
          print('found ' + slow.length + ' slow lines!');
          this.results.status = false;
          this.results.message += 'found ' + slow.length + ' slow lines!';
        } else {
          print('everything fast!');
        }
      }
    }
    catch(x) {
      print(x);
    }
  }
  
  runOneTest(testCase) {
    throw new Error("must overload the runOneTest function!");
  }
  ////////////////////////////////////////////////////////////////////////////////
  run(testList) {
    this.testList = testList;
    if (this.testList.length === 0) {
      print('Testsuite is empty!');
      return {
        "ALLTESTS" : {
          status: ((this.options.skipGrey ||
                    this.options.skipTimecritial ||
                    this.options.skipNondeterministic) &&
                   (this.options.test === undefined)),
          skipped: true,
          message: 'no testfiles were found!'
        }
      };
    }
    
    let beforeStart = time();

    this.customInstanceInfos['preStart'] = this.preStart();
    if (this.customInstanceInfos.preStart.state === false) {
      return {
        setup: {
          status: false,
          message: 'custom preStart failed!' + this.customInstanceInfos.preStart.message
        },
        shutdown: this.customInstanceInfos.preStart.shutdown
      };
    }

    this.instanceInfo = pu.startInstance(this.options.protocol,
                                         this.options,
                                         this.serverOptions,
                                         this.friendlyName);

    if (this.instanceInfo === false) {
      this.customInstanceInfos['startFailed'] = this.startFailed();
      return {
        setup: {
          status: false,
          message: 'failed to start server!'
        }
      };
    }

    this.customInstanceInfos['postStart'] = this.postStart();
    if (this.customInstanceInfos.postStart.state === false) {
      let shutdownStatus = this.customInstanceInfos.postStart.shutdown;
      shutdownStatus = shutdownStatus && pu.shutdownInstance(this.instanceInfo, this.options);
      return {
        setup: {
          status: false,
          message: 'custom postStart failed: ' + this.customInstanceInfos.postStart.message,
          shutdown: shutdownStatus
        }
      };
    }

    let testrunStart = time();
    this.results = {
      shutdown: true,
      startupTime: testrunStart - beforeStart
    };
    let serverDead = false;
    let count = 0;
    let forceTerminate = false;
    for (let i = 0; i < this.testList.length; i++) {
      let te = this.testList[i];
      let filtered = {};

      if (tu.filterTestcaseByOptions(te, this.options, filtered)) {
        let first = true;
        let loopCount = 0;
        count += 1;
        
        for (let j = 0; j < this.cleanupChecks.length; j++) {
          if (!this.continueTesting || !this.cleanupChecks[j].setUp(this, te)) {
            continue;
          }
        }
        while (first || this.options.loopEternal) {
          if (!this.continueTesting) {
            this.abortTestOnError(te);
            break;
          }

          this.preRun(te);
          pu.getMemProfSnapshot(this.instanceInfo, this.options, this.memProfCounter++);
          
          print('\n' + (new Date()).toISOString() + GREEN + " [============] " + this.info + ': Trying', te, '...', RESET);
          let reply = this.runOneTest(te);
          if (reply.hasOwnProperty('forceTerminate') && reply.forceTerminate) {
            this.results[te] = reply;
            this.continueTesting = false;
            forceTerminate = true;
            continue;
          }

          if (reply.hasOwnProperty('status')) {
            this.results[te] = reply;
            this.results[te]['processStats'] = pu.getDeltaProcessStats(this.instanceInfo);

            if (this.results[te].status === false) {
              this.options.cleanup = false;
            }

            if (!reply.status && !this.options.force) {
              break;
            }
          } else {
            this.results[te] = {
              status: false,
              message: reply
            };

            if (!this.options.force) {
              break;
            }
          }

          if (this.healthCheck()) {
            this.continueTesting = true;
            for (let j = 0; j < this.cleanupChecks.length; j++) {
              if (!this.continueTesting || this.cleanupChecks[j].runCheck(this, te)) {
                continue;
              }
            }
          }
          first = false;

          if (this.options.loopEternal) {
            if (loopCount % this.options.loopSleepWhen === 0) {
              print('sleeping...');
              sleep(this.options.loopSleepSec);
              print('continuing.');
            }

            ++loopCount;
          }
        }
      } else {
        if (this.options.extremeVerbosity) {
          print('Skipped ' + te + ' because of ' + filtered.filter);
        }
      }
    }

    if (count === 0) {
      this.results['ALLTESTS'] = {
        status: true,
        skipped: true
      };
      this.results.status = true;
      print(RED + 'No testcase matched the filter.' + RESET);
    }

    if (this.options.sleepBeforeShutdown !== 0) {
      print("Sleeping for " + this.options.sleepBeforeShutdown + " seconds");
      sleep(this.options.sleepBeforeShutdown);
    }

    if (this.continueTesting) {
      pu.getMemProfSnapshot(this.instanceInfo, this.options, this.memProfCounter++);
    }

    let shutDownStart = time();
    this.results['testDuration'] = shutDownStart - testrunStart;
    print(Date() + ' Shutting down...');
    this.customInstanceInfos.preStop = this.preStop();
    if (this.customInstanceInfos.preStop.state === false) {
      if (!this.results.hasOwnProperty('setup')) {
        this.results['setup'] = {};
      }
      this.results.setup['status'] = false;
      this.results.setup['message'] = 'custom preStop failed!' + this.customInstanceInfos.preStop.message;
      if (this.customInstanceInfos.preStop.hasOwnProperty('shutdown')) {
        this.results.shutdown = this.results.shutdown && this.customInstanceInfos.preStop.shutdown;
      }
    }
    // pass on JWT secret
    let clonedOpts = _.clone(this.options);
    if (this.serverOptions['server.jwt-secret'] && !clonedOpts['server.jwt-secret']) {
      clonedOpts['server.jwt-secret'] = this.serverOptions['server.jwt-secret'];
    }
    this.results.shutdown = this.results.shutdown && pu.shutdownInstance(this.instanceInfo, clonedOpts, forceTerminate);

    this.loadClusterTestStabilityInfo();

    this.customInstanceInfos['postStop'] = this.postStop();
    if (this.customInstanceInfos.postStop.state === false) {
      this.results.setup.status = false;
      this.results.setup.message = 'custom postStop failed!';
    }
    this.results['shutdownTime'] = time() - shutDownStart;

    print('done.');

    return this.results;
  }
}

exports.testRunner = testRunner;
exports.didSplitBuckets = function (val) { didSplitBuckets = val; };
