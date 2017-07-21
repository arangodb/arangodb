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

/* Modules: */
const _ = require('lodash');
const fs = require('fs');
const pu = require('@arangodb/process-utils');
const yaml = require('js-yaml');

const toArgv = require('internal').toArgv;
const time = require('internal').time;
const sleep = require('internal').sleep;
const download = require('internal').download;

/* Constants: */
// const BLUE = require('internal').COLORS.COLOR_BLUE;
// const CYAN = require('internal').COLORS.COLOR_CYAN;
// const GREEN = require('internal').COLORS.COLOR_GREEN;
const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;


let didSplitBuckets = false;


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
      'EMPTY TESTSUITE': {
        status: false,
        message: 'no testsuites found!'
      }
    };
  }

  if (serverOptions === undefined) {
    serverOptions = {};
  }

  let env = {};
  let customInstanceInfos = {};

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
        }
      };
    }
    _.defaults(env, customInstanceInfos.preStart.env);
  }

  let instanceInfo = pu.startInstance('tcp', options, serverOptions, testname);

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
      pu.shutdownInstance(instanceInfo, options);
      return {
        setup: {
          status: false,
          message: 'custom postStart failed: ' + customInstanceInfos.postStart.message
        }
      };
    }
    _.defaults(env, customInstanceInfos.postStart.env);
  }

  let results = {};
  let continueTesting = true;
  let count = 0;
  let forceTerminate = false;

  for (let i = 0; i < testList.length; i++) {
    let te = testList[i];
    let filtered = {};

    if (filterTestcaseByOptions(te, options, filtered)) {
      let first = true;
      let loopCount = 0;
      count += 1;

      while (first || options.loopEternal) {
        if (!continueTesting) {
          print('oops! Skipping, ' + te + ' server is gone.');

          results[te] = {
            status: false,
            message: instanceInfo.exitStatus
          };

          instanceInfo.exitStatus = 'server is gone.';

          break;
        }

        print('\n' + Date() + ' ' + runFn.info + ': Trying', te, '...');
        let reply = runFn(options, instanceInfo, te, env);

        if (reply.hasOwnProperty('forceTerminate')) {
          continueTesting = false;
          forceTerminate = true;
          continue;
        } else if (reply.hasOwnProperty('status')) {
          results[te] = reply;

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

        continueTesting = pu.arangod.check.instanceAlive(instanceInfo, options);
        if (startStopHandlers !== undefined && startStopHandlers.hasOwnProperty('alive')) {
          customInstanceInfos['alive'] = startStopHandlers.alive(options,
                                                                 serverOptions,
                                                                 instanceInfo,
                                                                 customInstanceInfos,
                                                                 startStopHandlers);
          if (customInstanceInfos.alive.state === false) {
            continueTesting = false;
            results.setup.message = 'custom preStop failed!';
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
      status: false,
      skipped: true
    };
    results.status = false;
    print(RED + 'No testcase matched the filter.' + RESET);
  }

  print('Shutting down...');
  if (startStopHandlers !== undefined && startStopHandlers.hasOwnProperty('preStop')) {
    customInstanceInfos['preStop'] = startStopHandlers.preStop(options,
                                                               serverOptions,
                                                               instanceInfo,
                                                               customInstanceInfos,
                                                               startStopHandlers);
    if (customInstanceInfos.preStop.state === false) {
      results.setup.status = false;
      results.setup.message = 'custom preStop failed!';
    }
  }

  // pass on JWT secret
  let clonedOpts = _.clone(options);
  if (serverOptions['server.jwt-secret'] && !clonedOpts['server.jwt-secret']) {
    clonedOpts['server.jwt-secret'] = serverOptions['server.jwt-secret'];
  }
  pu.shutdownInstance(instanceInfo, clonedOpts, forceTerminate);

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
  print('done.');

  return results;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief filter test-cases according to options
// //////////////////////////////////////////////////////////////////////////////

function filterTestcaseByOptions (testname, options, whichFilter) {
  // These filters require a proper setup, Even if we filter by testcase:
  if ((testname.indexOf('-mmfiles') !== -1) && options.storageEngine === 'rocksdb') {
    whichFilter.filter = 'skip when running as rocksdb';
    return false;
  }

  if ((testname.indexOf('-rocksdb') !== -1) && options.storageEngine === 'mmfiles') {
    whichFilter.filter = 'skip when running as mmfiles';
    return false;
  }

  if (options.replication) {
    whichFilter.filter = 'replication';

    if (options.hasOwnProperty('test') && (typeof (options.test) !== 'undefined')) {
      whichFilter.filter = 'testcase';
      return ((testname.search(options.test) >= 0) &&
              (testname.indexOf('replication') !== -1));
    } else {
      return testname.indexOf('replication') !== -1;
    }
  } else if (testname.indexOf('replication') !== -1) {
    whichFilter.filter = 'replication';
    return false;
  }

  if ((testname.indexOf('-cluster') !== -1) && !options.cluster) {
    whichFilter.filter = 'noncluster';
    return false;
  }

  if (testname.indexOf('-noncluster') !== -1 && options.cluster) {
    whichFilter.filter = 'cluster';
    return false;
  }

  // if we filter, we don't care about the other filters below:
  if (options.hasOwnProperty('test') && (typeof (options.test) !== 'undefined')) {
    whichFilter.filter = 'testcase';
    return testname.search(options.test) >= 0;
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

  return true;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief split into buckets
// //////////////////////////////////////////////////////////////////////////////

function splitBuckets (options, cases) {
  if (!options.testBuckets || cases.length === 0) {
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

  for (let i = s % m; i < cases.length; i = i + r) {
    result.push(cases[i]);
  }

  return result;
}

function doOnePathInner (path) {
  return _.filter(fs.list(makePathUnix(path)),
                  function (p) {
                    return (p.substr(-3) === '.js');
                  })
    .map(function (x) {
      return fs.join(makePathUnix(path), x);
    }).sort();
}

function scanTestPath (path) {
  var community = doOnePathInner(path);
  if (global.ARANGODB_CLIENT_VERSION(true)['enterprise-version']) {
    return community.concat(doOnePathInner('enterprise/' + path));
  } else {
    return community;
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief runs a remote unittest file using /_admin/execute
// //////////////////////////////////////////////////////////////////////////////

function runThere (options, instanceInfo, file) {
  try {
    let testCode;
    let mochaGrep = options.mochaGrep ? ', ' + JSON.stringify(options.mochaGrep) : '';
    if (file.indexOf('-spec') === -1) {
      testCode = 'const runTest = require("jsunity").runTest; ' +
        'return runTest(' + JSON.stringify(file) + ', true);';
    } else {
      testCode = 'const runTest = require("@arangodb/mocha-runner"); ' +
        'return runTest(' + JSON.stringify(file) + ', true' + mochaGrep + ');';
    }

    testCode = 'global.instanceInfo = ' + JSON.stringify(instanceInfo) + ';\n' + testCode;

    let httpOptions = pu.makeAuthorizationHeaders(options);
    httpOptions.method = 'POST';
    httpOptions.timeout = 1800;

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
          (reply.message === 'Request timeout reached')) {
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
runThere.info = 'runThere';

// //////////////////////////////////////////////////////////////////////////////
// / @brief runs a local unittest file using arangosh
// //////////////////////////////////////////////////////////////////////////////

function runInArangosh (options, instanceInfo, file, addArgs) {
  let args = pu.makeArgs.arangosh(options);
  args['server.endpoint'] = instanceInfo.endpoint;

  args['javascript.unit-tests'] = fs.join(pu.TOP_DIR, file);

  if (!options.verbose) {
    args['log.level'] = 'warning';
  }

  if (addArgs !== undefined) {
    args = Object.assign(args, addArgs);
  }
  require('internal').env.INSTANCEINFO = JSON.stringify(instanceInfo);
  let rc = pu.executeAndWait(pu.ARANGOSH_BIN, toArgv(args), options, 'arangosh', instanceInfo.rootDir);
  let result;
  try {
    result = JSON.parse(fs.read(instanceInfo.rootDir + '/testresult.json'));

    fs.remove(instanceInfo.rootDir + '/testresult.json');
  } catch (x) {
    if (options.extremeVerbosity) {
      print('failed to read ' + instanceInfo.rootDir + '/testresult.json');
    }
    return rc;
  }

  if ((typeof result[0] === 'object') &&
    result[0].hasOwnProperty('status')) {
    return result[0];
  } else {
    rc.failed = rc.status ? 0 : 1;
    return rc;
  }
}
runInArangosh.info = 'arangosh';

function makeResults (testname, instanceInfo) {
  const startTime = time();

  return function (status, message) {
    let duration = time() - startTime;
    let results;

    if (status) {
      let result;

      try {
        result = JSON.parse(fs.read(instanceInfo.rootDir + '/testresult.json'));

        if ((typeof result[0] === 'object') &&
            result[0].hasOwnProperty('status')) {
          results = result[0];
        }
      } catch (x) {}
    }

    if (results === undefined) {
      results = {
        status: status,
        duration: duration,
        total: 1,
        failed: status ? 0 : 1,
        'testing.js': {
          status: status,
          duration: duration
        }
      };

      if (message) {
        results['testing.js'].message = message;
      }
    }

    let full = {};
    full[testname] = results;

    return full;
  };
}

exports.runThere = runThere;
exports.runInArangosh = runInArangosh;

exports.makePathUnix = makePathUnix;
exports.makePathGeneric = makePathGeneric;
exports.performTests = performTests;
exports.filterTestcaseByOptions = filterTestcaseByOptions;
exports.splitBuckets = splitBuckets;
exports.doOnePathInner = doOnePathInner;
exports.scanTestPath = scanTestPath;
exports.makeResults = makeResults;
