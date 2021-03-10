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
// / @author Max Neunhoeffer
// //////////////////////////////////////////////////////////////////////////////

/* Modules: */
const _ = require('lodash');
const fs = require('fs');
const pu = require('@arangodb/testutils/process-utils');
const yaml = require('js-yaml');

const toArgv = require('internal').toArgv;
const time = require('internal').time;
const sleep = require('internal').sleep;
const download = require('internal').download;
const pathForTesting = require('internal').pathForTesting;
const platform = require('internal').platform;
const SetGlobalExecutionDeadlineTo = require('internal').SetGlobalExecutionDeadlineTo;
/* Constants: */
// const BLUE = require('internal').COLORS.COLOR_BLUE;
// const CYAN = require('internal').COLORS.COLOR_CYAN;
const GREEN = require('internal').COLORS.COLOR_GREEN;
const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
const YELLOW = require('internal').COLORS.COLOR_YELLOW;

let didSplitBuckets = false;

// //////////////////////////////////////////////////////////////////////////////
// / @brief get the items uniq to arr1 or arr2
// //////////////////////////////////////////////////////////////////////////////

function diffArray (arr1, arr2) {
  return arr1.concat(arr2).filter(function (val) {
    if (!(arr1.includes(val) && arr2.includes(val))) {
      return val;
    }
  });
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief build a unix path
// //////////////////////////////////////////////////////////////////////////////

function makePathUnix (path) {
  return fs.join.apply(null, path.split('/'));
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief build a generic path
// //////////////////////////////////////////////////////////////////////////////

function makePathGeneric (path) {
  return fs.join.apply(null, path.split(fs.pathSeparator));
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief runs a list of tests
// //////////////////////////////////////////////////////////////////////////////

function performTests (options, testList, testname, runFn, serverOptions, startStopHandlers) {
  if (options.testBuckets && !didSplitBuckets) {
    throw new Error("You parametrized to split buckets, but this testsuite doesn't support it!!!");
  }

  if (testList.length === 0) {
    print('Testsuite is empty!');
    return {
      "ALLTESTS" : {
        status: ((options.skipGrey || options.skipTimecritial || options.skipNondeterministic) && (options.test === undefined)),
        skipped: true,
        message: 'no testfiles were found!'
      }
    };
  }

  if (serverOptions === undefined) {
    serverOptions = {};
  }

  let memProfCounter = 0;
  let env = {};
  let customInstanceInfos = {};
  let healthCheck = function () {return true;};
  let beforeStart = time();
  if (startStopHandlers !== undefined && startStopHandlers.hasOwnProperty('healthCheck')) {
    healthCheck = startStopHandlers.healthCheck;
  }

  if (startStopHandlers !== undefined && startStopHandlers.hasOwnProperty('preStart')) {
    customInstanceInfos['preStart'] = startStopHandlers.preStart(options,
                                                                 serverOptions,
                                                                 customInstanceInfos,
                                                                 startStopHandlers);
    if (customInstanceInfos.preStart.state === false) {
      return {
        setup: {
          status: false,
          message: 'custom preStart failed!' + customInstanceInfos.preStart.message
        },
        shutdown: customInstanceInfos.preStart.shutdown
      };
    }
    _.defaults(env, customInstanceInfos.preStart.env);
  }

  let instanceInfo = pu.startInstance(options.protocol, options, serverOptions, testname);

  if (instanceInfo === false) {
    if (startStopHandlers !== undefined && startStopHandlers.hasOwnProperty('startFailed')) {
      customInstanceInfos['startFailed'] = startStopHandlers.startFailed(options,
                                                                         serverOptions,
                                                                         customInstanceInfos,
                                                                         startStopHandlers);
    }
    return {
      setup: {
        status: false,
        message: 'failed to start server!'
      }
    };
  }

  if (startStopHandlers !== undefined && startStopHandlers.hasOwnProperty('postStart')) {
    customInstanceInfos['postStart'] = startStopHandlers.postStart(options,
                                                                   serverOptions,
                                                                   instanceInfo,
                                                                   customInstanceInfos,
                                                                   startStopHandlers);
    if (customInstanceInfos.postStart.state === false) {
      let shutdownStatus = customInstanceInfos.postStart.shutdown;
      shutdownStatus = shutdownStatus && pu.shutdownInstance(instanceInfo, options);
      return {
        setup: {
          status: false,
          message: 'custom postStart failed: ' + customInstanceInfos.postStart.message,
          shutdown: shutdownStatus
        }
      };
    }
    _.defaults(env, customInstanceInfos.postStart.env);
  }

  let testrunStart = time();
  let results = {
    shutdown: true,
    startupTime: testrunStart - beforeStart
  };
  let continueTesting = true;
  let serverDead = false;
  let count = 0;
  let forceTerminate = false;
  let graphCount = 0;

  for (let i = 0; i < testList.length; i++) {
    let te = testList[i];
    let filtered = {};

    if (filterTestcaseByOptions(te, options, filtered)) {
      let first = true;
      let loopCount = 0;
      count += 1;

      let collectionsBefore = [];
      let viewsBefore = [];
      if (!serverDead) {
        try {
          db._collections().forEach(collection => {
            collectionsBefore.push(collection._name);
          });
          db._views().forEach(view => {
            viewsBefore.push(view._name);
          });
        }
        catch (x) {
          results[te] = {
            status: false,
            message: 'failed to fetch the previously available collections & views: ' + x.message
          };
          continueTesting = false;
          serverDead = true;
          first = false;
        }
      }
      while (first || options.loopEternal) {
        if (!continueTesting) {

          if (!results.hasOwnProperty('SKIPPED')) {
            print('oops! Skipping remaining tests, server is unavailable for testing.');
            let originalMessage;
            if (results.hasOwnProperty(te) && results[te].hasOwnProperty('message')) {
              originalMessage = results[te].message;
            }
            results['SKIPPED'] = {
              status: false,
              message: ""
            };
            results[te] = {
              status: false,
              message: 'server unavailable for testing. ' + originalMessage
            };
          } else {
            if (results['SKIPPED'].message !== '') {
              results['SKIPPED'].message += ', ';
            }
            results['SKIPPED'].message += te;
          }

          instanceInfo.exitStatus = 'server is gone.';

          break;
        }

        pu.getMemProfSnapshot(instanceInfo, options, memProfCounter++);
        print('\n' + (new Date()).toISOString() + GREEN + " [============] " + runFn.info + ': Trying', te, '...', RESET);
        let reply = runFn(options, instanceInfo, te, env);

        if (reply.hasOwnProperty('forceTerminate')) {
          results[te] = reply;
          continueTesting = false;
          forceTerminate = true;
          continue;
        } else if (reply.hasOwnProperty('status')) {
          results[te] = reply;
          results[te]['processStats'] = pu.getDeltaProcessStats(instanceInfo);

          if (results[te].status === false) {
            options.cleanup = false;
          }

          if (!reply.status && !options.force) {
            break;
          }
        } else {
          results[te] = {
            status: false,
            message: reply
          };

          if (!options.force) {
            break;
          }
        }
        if (pu.arangod.check.instanceAlive(instanceInfo, options) &&
            healthCheck(options, serverOptions, instanceInfo, customInstanceInfos, startStopHandlers)) {
          continueTesting = true;

          // Check whether some collections & views were left behind, and if mark test as failed.
          let collectionsAfter = [];
          let viewsAfter = [];
          try {
            db._collections().forEach(collection => {
              collectionsAfter.push(collection._name);
            });
            db._views().forEach(view => {
              viewsAfter.push(view._name);
            });
          }
          catch (x) {
            results[te] = {
              status: false,
              message: 'failed to fetch the currently available collections & views: ' + x.message + '. Original test status: ' + JSON.stringify(results[te])
            };
            continueTesting = false;
            continue;
          }
          let delta = diffArray(collectionsBefore, collectionsAfter).filter(function(name) {
            return ! ((name[0] === '_') || (name === "compact") || (name === "election")
                     || (name === "log")); // exclude system/agency collections from the comparison
            return (name[0] !== '_'); // exclude system collections from the comparison
          });

          let deltaViews = diffArray(viewsBefore, viewsAfter).filter(function(name) {
            return ! ((name[0] === '_') || (name === "compact") || (name === "election")
                     || (name === "log"));
          });
          if ((delta.length !== 0) || (deltaViews.length !== 0)){
            results[te] = {
              status: false,
              message: 'Cleanup missing - test left over collections / views : ' + delta + ' / ' + deltaViews + '. Original test status: ' + JSON.stringify(results[te])
            };
            collectionsBefore = [];
            viewsBefore = [];
            try {
              db._collections().forEach(collection => {
                collectionsBefore.push(collection._name);
              });
              db._views().forEach(view => {
                viewsBefore.push(view._name);
              });
            }
            catch (x) {
              results[te] = {
                status: false,
                message: 'failed to fetch the currently available delta collections / views: ' + x.message + '. Original test status: ' + JSON.stringify(results[te])
              };
              continueTesting = false;
              continue;
            }
          }

          let graphs;
          try {
            graphs = db._collection('_graphs');
          }
          catch (x) {
            results[te] = {
              status: false,
              message: 'failed to fetch the graphs: ' + x.message + '. Original test status: ' + JSON.stringify(results[te])
            };
            continueTesting = false;
            continue;
          }
          if (graphs && graphs.count() !== graphCount) {
            results[te] = {
              status: false,
              message: 'Cleanup of graphs missing - found graph definitions: [ ' +
                JSON.stringify(graphs.toArray()) +
                ' ] - Original test status: ' +
                JSON.stringify(results[te])
            };
            graphCount = graphs.count();
          }
        } else {
          serverDead = true;
          continueTesting = false;
          let msg = '';
          if (results[te].hasOwnProperty('message') && results[te].message.length > 0) {
            msg = results[te].message + ' - ';
          }
          results[te] = {
            status: false,
            message: 'server is dead: ' + msg + instanceInfo.message
          };
        }

        if (startStopHandlers !== undefined && startStopHandlers.hasOwnProperty('alive')) {
          customInstanceInfos['alive'] = startStopHandlers.alive(options,
                                                                 serverOptions,
                                                                 instanceInfo,
                                                                 customInstanceInfos,
                                                                 startStopHandlers);
          if (customInstanceInfos.alive.state === false) {
            continueTesting = false;
            results.setup.message = 'custom alive check failed!';
          }
          collectionsBefore = [];
          viewsBefore = [];
          try {
            db._collections().forEach(collection => {
              collectionsBefore.push(collection._name);
            });
            db._views().forEach(view => {
              viewsBefore.push(view._name);
            });
          }
          catch (x) {
            results[te] = {
              status: false,
              message: 'failed to fetch the currently available shutdown collections / views: ' + x.message + '. Original test status: ' + JSON.stringify(results[te])
            };
            continueTesting = false;
            continue;
          }

        }

        first = false;

        if (options.loopEternal) {
          if (loopCount % options.loopSleepWhen === 0) {
            print('sleeping...');
            sleep(options.loopSleepSec);
            print('continuing.');
          }

          ++loopCount;
        }
      }
    } else {
      if (options.extremeVerbosity) {
        print('Skipped ' + te + ' because of ' + filtered.filter);
      }
    }
  }

  if (count === 0) {
    results['ALLTESTS'] = {
      status: true,
      skipped: true
    };
    results.status = true;
    print(RED + 'No testcase matched the filter.' + RESET);
  }

  if (options.sleepBeforeShutdown !== 0) {
    print("Sleeping for " + options.sleepBeforeShutdown + " seconds");
    sleep(options.sleepBeforeShutdown);
  }

  if (continueTesting) {
    pu.getMemProfSnapshot(instanceInfo, options, memProfCounter++);
  }

  let shutDownStart = time();
  results['testDuration'] = shutDownStart - testrunStart;
  print(Date() + ' Shutting down...');
  if (startStopHandlers !== undefined && startStopHandlers.hasOwnProperty('preStop')) {
    customInstanceInfos['preStop'] = startStopHandlers.preStop(options,
                                                               serverOptions,
                                                               instanceInfo,
                                                               customInstanceInfos,
                                                               startStopHandlers);
    if (customInstanceInfos.preStop.state === false) {
      if (!results.hasOwnProperty('setup')) {
        results['setup'] = {};
      }
      results.setup['status'] = false;
      results.setup['message'] = 'custom preStop failed!' + customInstanceInfos.preStop.message;
      if (customInstanceInfos.preStop.hasOwnProperty('shutdown')) {
        results.shutdown = results.shutdown && customInstanceInfos.preStop.shutdown;
      }
    }
  }

  // pass on JWT secret
  let clonedOpts = _.clone(options);
  if (serverOptions['server.jwt-secret'] && !clonedOpts['server.jwt-secret']) {
    clonedOpts['server.jwt-secret'] = serverOptions['server.jwt-secret'];
  }
  results.shutdown = results.shutdown && pu.shutdownInstance(instanceInfo, clonedOpts, forceTerminate);

  loadClusterTestStabilityInfo(results, instanceInfo);

  if (startStopHandlers !== undefined && startStopHandlers.hasOwnProperty('postStop')) {
    customInstanceInfos['postStop'] = startStopHandlers.postStop(options,
                                                                 serverOptions,
                                                                 instanceInfo,
                                                                 customInstanceInfos,
                                                                 startStopHandlers);
    if (customInstanceInfos.postStop.state === false) {
      results.setup.status = false;
      results.setup.message = 'custom postStop failed!';
    }
  }
  results['shutdownTime'] = time() - shutDownStart;

  print('done.');

  return results;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief filter test-cases according to options
// //////////////////////////////////////////////////////////////////////////////

function filterTestcaseByOptions (testname, options, whichFilter) {
  // These filters require a proper setup, Even if we filter by testcase:
  if ((testname.indexOf('-cluster') !== -1) && !options.cluster) {
    whichFilter.filter = 'noncluster';
    return false;
  }

  if (testname.indexOf('-noncluster') !== -1 && options.cluster) {
    whichFilter.filter = 'cluster';
    return false;
  }

  if (testname.indexOf('-arangosearch') !== -1 && !options.arangosearch) {
    whichFilter.filter = 'arangosearch';
    return false;
  }

  // if we filter, we don't care about the other filters below:
  if (options.hasOwnProperty('test') && (typeof (options.test) !== 'undefined')) {
    whichFilter.filter = 'testcase';
    if (typeof options.test === 'string') {
      return testname.search(options.test) >= 0;
    } else {
      let found = false;
      options.test.forEach(
        filter => {
          if (testname.search(filter) >= 0) {
            found = true;
          }
        }
      );
      return found;
    }
  }

  if (testname.indexOf('-timecritical') !== -1 && options.skipTimeCritical) {
    whichFilter.filter = 'timecritical';
    return false;
  }

  if (testname.indexOf('-nightly') !== -1 && options.skipNightly && !options.onlyNightly) {
    whichFilter.filter = 'skip nightly';
    return false;
  }

  if (testname.indexOf('-geo') !== -1 && options.skipGeo) {
    whichFilter.filter = 'geo';
    return false;
  }

  if (testname.indexOf('-nondeterministic') !== -1 && options.skipNondeterministic) {
    whichFilter.filter = 'nondeterministic';
    return false;
  }

  if (testname.indexOf('-grey') !== -1 && options.skipGrey) {
    whichFilter.filter = 'grey';
    return false;
  }

  if (testname.indexOf('-grey') === -1 && options.onlyGrey) {
    whichFilter.filter = 'grey';
    return false;
  }

  if (testname.indexOf('-graph') !== -1 && options.skipGraph) {
    whichFilter.filter = 'graph';
    return false;
  }

  if (testname.indexOf('-disabled') !== -1) {
    whichFilter.filter = 'disabled';
    return false;
  }

  if ((testname.indexOf('-memoryintense') !== -1) && options.skipMemoryIntense) {
    whichFilter.filter = 'memoryintense';
    return false;
  }

  if (testname.indexOf('-nightly') === -1 && options.onlyNightly) {
    whichFilter.filter = 'only nightly';
    return false;
  }

  if ((testname.indexOf('-novalgrind') !== -1) && options.valgrind) {
    whichFilter.filter = 'skip in valgrind';
    return false;
  }

  if ((testname.indexOf('-noasan') !== -1) && global.ARANGODB_CLIENT_VERSION(true).asan === 'true') {
    whichFilter.filter = 'skip when built with asan';
    return false;
  }

  if (options.failed) {
    return options.failed.hasOwnProperty(testname);
  }
  return true;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief split into buckets
// //////////////////////////////////////////////////////////////////////////////

function splitBuckets (options, cases) {
  if (!options.testBuckets) {
    return cases;
  }
  if (cases.length === 0) {
    didSplitBuckets = true;
    return cases;
  }

  didSplitBuckets = true;
  let m = cases.length;
  let n = options.testBuckets.split('/');
  let r = parseInt(n[0]);
  let s = parseInt(n[1]);

  if (r < 1) {
    r = 1;
  }

  if (r === 1) {
    return cases;
  }

  if (s < 0) {
    s = 0;
  }
  if (r <= s) {
    s = r - 1;
  }

  let result = [];

  cases.sort();

  for (let i = s % m; i < cases.length; i = i + r) {
    result.push(cases[i]);
  }

  return result;
}

function doOnePathInner (path) {
  return _.filter(fs.list(makePathUnix(path)),
                  function (p) {
                    return (p.substr(-3) === '.js') || (p.substr(-3) === '.rb');;
                  })
    .map(function (x) {
      return fs.join(makePathUnix(path), x);
    }).sort();
}

function scanTestPaths (paths, options) {
  // add Enterprise Edition tests
  if (global.ARANGODB_CLIENT_VERSION(true)['enterprise-version']) {
    paths = paths.concat(paths.map(function(p) {
      return 'enterprise/' + p;
    }));
  }

  let allTestCases = [];

  paths.forEach(function(p) {
    allTestCases = allTestCases.concat(doOnePathInner(p));
  });

  let allFiltered = [];
  let filteredTestCases = _.filter(allTestCases,
                                   function (p) {
                                     let whichFilter = {};
                                     let rc = filterTestcaseByOptions(p, options, whichFilter);
                                     if (!rc) {
                                       allFiltered.push(p + " Filtered by: " + whichFilter.filter);
                                     }
                                     return rc;
                                   });
  if ((filteredTestCases.length === 0) && (options.extremeVerbosity !== 'silence')) {
    print("No testcase matched the filter: " + JSON.stringify(allFiltered));
    return [];
  }

  return allTestCases;
}


function loadClusterTestStabilityInfo(results, instanceInfo){
  try {
    if (instanceInfo.hasOwnProperty('clusterHealthMonitorFile')) {
      let status = true;
      let slow = [];
      let buf = fs.readBuffer(instanceInfo.clusterHealthMonitorFile);
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
        results.status = false;
        results.message += 'found ' + slow.length + ' slow lines!';
      } else {
        print('everything fast!');
      }
    }
  }
  catch(x) {
    print(x);
  }
}

function getTestCode(file, options, instanceInfo) {
  let filter;
  if (options.testCase) {
    filter = JSON.stringify(options.testCase);
  } else if (options.failed) {
    let failed = options.failed[file] || options.failed;
    filter = JSON.stringify(Object.keys(failed));
  }

  let runTest;
  if (file.indexOf('-spec') === -1) {
    filter = filter || '"undefined"';
    runTest = 'const runTest = require("jsunity").runTest;\n';

  } else {
    filter = filter || '';
    runTest = 'const runTest = require("@arangodb/mocha-runner");\n';
  }
  return 'global.instanceInfo = ' + JSON.stringify(instanceInfo) + ';\n' + runTest +
         'return runTest(' + JSON.stringify(file) + ', true, ' + filter + ');\n';
}
// //////////////////////////////////////////////////////////////////////////////
// / @brief runs a remote unittest file using /_admin/execute
// //////////////////////////////////////////////////////////////////////////////

function runThere (options, instanceInfo, file) {
  try {
    let testCode = getTestCode(file, options, instanceInfo);
    let httpOptions = pu.makeAuthorizationHeaders(options);
    httpOptions.method = 'POST';

    httpOptions.timeout = options.oneTestTimeout;
    if (options.isAsan) {
      httpOptions.timeout *= 2;
    }
    if (options.valgrind) {
      httpOptions.timeout *= 2;
    }

    httpOptions.returnBodyOnError = true;

    const reply = download(instanceInfo.url + '/_admin/execute?returnAsJSON=true',
      testCode,
      httpOptions);

    if (!reply.error && reply.code === 200) {
      return JSON.parse(reply.body);
    } else {
      if ((reply.code === 500) &&
          reply.hasOwnProperty('message') &&
          (
            (reply.message.search('Request timeout reached') >= 0 ) ||
            (reply.message.search('timeout during read') >= 0 ) ||
            (reply.message.search('Connection closed by remote') >= 0 )
          )) {
        print(RED + Date() + " request timeout reached (" + reply.message +
              "), aborting test execution" + RESET);
        return {
          status: false,
          message: reply.message,
          forceTerminate: true
        };
      } else if (reply.hasOwnProperty('body')) {
        return {
          status: false,
          message: reply.body
        };
      } else {
        return {
          status: false,
          message: yaml.safeDump(reply)
        };
      }
    }
  } catch (ex) {
    return {
      status: false,
      message: ex.message || String(ex),
      stack: ex.stack
    };
  }
}
runThere.info = 'runInArangod';

function readTestResult(path, rc, testCase) {
  const jsonFN = fs.join(path, 'testresult.json');
  let buf;
  try {
    buf = fs.read(jsonFN);
    fs.remove(jsonFN);
  } catch (x) {
    let msg = 'readTestResult: failed to read ' + jsonFN + " - " + x;
    print(RED + msg + RESET);
    rc.message += " - " + msg;
    rc.status = false;
    return rc;
  }

  let result;
  try {
    result = JSON.parse(buf);
  } catch (x) {
    let msg = 'readTestResult: failed to parse ' + jsonFN + "'" + buf + "' - " + x;
    print(RED + msg + RESET);
    rc.message += " - " + msg;
    rc.status = false;
    return rc;
  }

  if (Array.isArray(result)) {
    if (result.length === 0) {
      // spec-files - don't have parseable results.
      rc.failed = rc.status ? 0 : 1;
      return rc;
    } else if ((result.length >= 1) &&
               (typeof result[0] === 'object') &&
               result[0].hasOwnProperty('status')) {
      return result[0];
    } else {
      rc.failed = rc.status ? 0 : 1;
      rc.message = "don't know howto handle '" + buf + "'";
      return rc;
    }
  } else if (_.isObject(result)) {
    if ((testCase !== undefined) && result.hasOwnProperty(testCase)) {
      return result[testCase];
    } else {
      return result;
    }
  } else {
    rc.failed = rc.status ? 0 : 1;
    rc.message = "readTestResult: don't know howto handle '" + buf + "'";
    return rc;
  }
}

function writeTestResult(path, data) {
  const jsonFN = fs.join(path, 'testresult.json');
  fs.write(jsonFN, JSON.stringify(data));
}

/// make arangosh use HTTP, VST or HTTP2 according to option
function findEndpoint(options, instanceInfo) {
  let endpoint = instanceInfo.endpoint;
  if (options.vst) {
    endpoint = endpoint.replace(/.*\/\//, 'vst://');
  } else if (options.http2) {
    endpoint = endpoint.replace(/.*\/\//, 'h2://');
  }
  print("using endpoint ", endpoint);
  return endpoint;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief runs a local unittest file using arangosh
// //////////////////////////////////////////////////////////////////////////////

function runInArangosh (options, instanceInfo, file, addArgs) {
  let args = pu.makeArgs.arangosh(options);
  args['server.endpoint'] = findEndpoint(options, instanceInfo);

  args['javascript.unit-tests'] = fs.join(pu.TOP_DIR, file);

  args['javascript.unit-test-filter'] = options.testCase;

  if (options.forceJson) {
    args['server.force-json'] = true;
  }

  if (!options.verbose) {
    args['log.level'] = 'warning';
  }

  if (addArgs !== undefined) {
    args = Object.assign(args, addArgs);
  }
  require('internal').env.INSTANCEINFO = JSON.stringify(instanceInfo);
  let rc = pu.executeAndWait(pu.ARANGOSH_BIN, toArgv(args), options, 'arangosh', instanceInfo.rootDir, options.coreCheck);
  return readTestResult(instanceInfo.rootDir, rc, args['javascript.unit-tests']);
}
runInArangosh.info = 'runInExternalArangosh';

// //////////////////////////////////////////////////////////////////////////////
// / @brief runs a local unittest file in the current arangosh
// //////////////////////////////////////////////////////////////////////////////

function runInLocalArangosh (options, instanceInfo, file, addArgs) {
  let endpoint = arango.getEndpoint();
  if (options.vst || options.http2) {
    let newEndpoint = findEndpoint(options, instanceInfo);
    if (endpoint !== newEndpoint) {
      print(`runInLocalArangosh: Reconnecting to ${newEndpoint} from ${endpoint}`);
      arango.reconnect(newEndpoint, '_system', 'root', '');
    }
  }

  let testCode = getTestCode(file, options, instanceInfo);
  require('internal').env.INSTANCEINFO = JSON.stringify(instanceInfo);
  let testFunc;
  try {
    eval('testFunc = function () {\n' + testCode + "}");
  } catch (ex) {
    print(RED + 'test failed to parse:');
    print(ex);
    print(RESET);
    return {
      status: false,
      message: "test doesn't parse! '" + file + "' - " + ex.message || String(ex),
      stack: ex.stack
    };
  }

  try {
    SetGlobalExecutionDeadlineTo(options.oneTestTimeout * 1000);
    let result = testFunc();
    let timeout = SetGlobalExecutionDeadlineTo(0.0);
    if (timeout) {
      return {
        timeout: true,
        forceTerminate: true,
        status: false,
        message: "test ran into timeout. Original test status: " + JSON.stringify(result),
      };
    }
    if (result === undefined) {
      return {
        timeout: true,
        status: false,
        message: "test didn't return any result at all!"
      };
    }
    return result;
  } catch (ex) {
    let timeout = SetGlobalExecutionDeadlineTo(0.0);
    print(RED + 'test has thrown: ' + (timeout? "because of timeout in execution":""));
    print(ex);
    print(RESET);
    return {
      timeout: timeout,
      forceTerminate: true,
      status: false,
      message: "test has thrown! '" + file + "' - " + ex.message || String(ex),
      stack: ex.stack
    };
  }
}
runInLocalArangosh.info = 'runInLocalArangosh';

const parseRspecJson = function (testCase, res, totalDuration) {
  let tName = testCase.description;
  let status = (testCase.status === 'passed');

  if (res.hasOwnProperty(tName)) {
    throw new Error(`duplicate testcase name in ${tName} ${JSON.stringify(testCase)}`);
  }
  res[tName] = {
    status: status,
    message: testCase.full_description,
    duration: totalDuration // RSpec doesn't offer per testcase time...
  };

  res.total++;

  if (!status) {
    const msg = yaml.safeDump(testCase)
          .replace(/.*rspec\/core.*\n/gm, '')
          .replace(/.*rspec\\core.*\n/gm, '')
          .replace(/.*lib\/ruby.*\n/, '')
          .replace(/.*- >-.*\n/gm, '')
          .replace(/\n *`/gm, ' `');
    print('RSpec test case falied: \n' + msg);
    res[tName].message += '\n' + msg;
  }
  return status ? 0 : 1;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief runs a unittest file using rspec
// //////////////////////////////////////////////////////////////////////////////
function runInRSpec (options, instanceInfo, file, addArgs) {
  const tmpname = fs.join(instanceInfo.rootDir, 'testconfig.rb');
  const jsonFN = fs.join(instanceInfo.rootDir, 'testresult.json');

  let command;
  let args;
  let rspec;
  let ssl = "0";
  if (instanceInfo.protocol === 'ssl') {
    ssl = "1";
  }
  let rspecConfig = 'RSpec.configure do |c|\n' +
                    '  c.add_setting :ARANGO_SERVER\n' +
      '  c.ARANGO_SERVER = "' +
      instanceInfo.endpoint.substr(6) + '"\n' +
      '  c.add_setting :ARANGO_SSL\n' +
      '  c.ARANGO_SSL = "' + ssl + '"\n' +
      '  c.add_setting :ARANGO_USER\n' +
      '  c.ARANGO_USER = "' + options.username + '"\n' +
      '  c.add_setting :ARANGO_PASSWORD\n' +
      '  c.ARANGO_PASSWORD = "' + options.password + '"\n' +
      '  c.add_setting :SKIP_TIMECRITICAL\n' +
      '  c.SKIP_TIMECRITICAL = ' + JSON.stringify(options.skipTimeCritical) + '\n' +
      '  c.add_setting :STORAGE_ENGINE\n' +
      '  c.STORAGE_ENGINE = ' + JSON.stringify(options.storageEngine) + '\n' +
      'end\n';

  fs.write(tmpname, rspecConfig);
  if (options.hasOwnProperty('ruby')) {
    let rx = new RegExp('ruby.exe$');
    rspec = options.ruby.replace(rx, 'rspec');
    command = options.ruby;
  } else {
    if (platform.substr(0, 3) !== 'win') {
      command = 'rspec';
    } else {
      // Windows process utilties would apply `.exe` to rspec.
      // However the file is called .bat and .exe cannot be found.
      command = 'rspec.bat';
    }
  }

  if (options.extremeVerbosity === true) {
    print('rspecConfig: \n' + rspecConfig);
  }

  try {
    fs.makeDirectory(pu.LOGS_DIR);
  } catch (err) {}

  args = ['--color',
          '-I', fs.join('tests', 'arangodbRspecLib'),
          '--format', 'd',
          '--format', 'j',
          '--out', jsonFN,
          '--require', tmpname];

  if (options.testCase) {
    args.push("--example-matches");
    args.push(options.testCase + "$");
  } else if (options.failed) {
    let failed = Object.keys(options.failed[file]);
    args.push("--example-matches");
    let escapeRegExp = function(str) {
      return str.replace(/([.*+?^${}()|[\]\/\\])/g, '\\$1');
    };
    args.push(failed.map(v => escapeRegExp(v)).join("$|") + "$");
  }
  args.push(file);

  if (rspec !== undefined) {
    args = [rspec].concat(args);
  }

  let start = Date();
  const res = pu.executeAndWait(command, args, options, 'rspec', instanceInfo.rootDir, false, options.oneTestTimeout);
  let end = Date();

  if (!res.status && res.timeout) {
    return {
      total: 0,
      failed: 1,
      status: false,
      forceTerminate: true,
      message: res.message
    };
  }

  let result = {
    total: 0,
    failed: 0,
    duration: end - start,
    status: res.status
  };

  try {
    const jsonResult = JSON.parse(fs.read(jsonFN));

    if (options.extremeVerbosity) {
      print(yaml.safeDump(jsonResult));
    }
    for (let j = 0; j < jsonResult.examples.length; ++j) {
      result.failed += parseRspecJson(
        jsonResult.examples[j],
        result,
        jsonResult.summary.duration);
    }

    result.duration = jsonResult.summary.duration;
  } catch (x) {
    result.failed = 1;
    result.status = false;
    result.message = 'Failed to parse rspec results for: ' + file + ' - ' + x.message;

    if (res.status === false) {
      options.cleanup = false;
    }
  }

  if (options.cleanup) {
    if (fs.exists(jsonFN)) fs.remove(jsonFN);
    fs.remove(tmpname);
  }
  return result;
}
runInRSpec.info = 'runInLocalRSpec';

exports.runThere = runThere;
exports.runInArangosh = runInArangosh;
exports.runInLocalArangosh = runInLocalArangosh;
exports.runInRSpec = runInRSpec;

exports.makePathUnix = makePathUnix;
exports.makePathGeneric = makePathGeneric;
exports.performTests = performTests;
exports.readTestResult = readTestResult;
exports.writeTestResult = writeTestResult;
exports.filterTestcaseByOptions = filterTestcaseByOptions;
exports.splitBuckets = splitBuckets;
exports.doOnePathInner = doOnePathInner;
exports.scanTestPaths = scanTestPaths;
exports.diffArray = diffArray;
exports.pathForTesting = pathForTesting;
exports.findEndpoint = findEndpoint;
