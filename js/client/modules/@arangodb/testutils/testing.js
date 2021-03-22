/* jshint strict: false, sub: true */
/* global print, arango */
'use strict';

// /////////////////////////////////////////////////////////////////////////////
// DISCLAIMER
// 
// Copyright 2016-2018 ArangoDB GmbH, Cologne, Germany
// Copyright 2014 triagens GmbH, Cologne, Germany
// 
// Licensed under the Apache License, Version 2.0 (the "License")
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// 
// Copyright holder is ArangoDB GmbH, Cologne, Germany
// 
// @author Max Neunhoeffer
// @author Wilfried Goesgnes
// /////////////////////////////////////////////////////////////////////////////

const _ = require('lodash');
const fs = require('fs');

const pu = require('@arangodb/testutils/process-utils');
const rp = require('@arangodb/testutils/result-processing');
const cu = require('@arangodb/testutils/crash-utils');
const tu = require('@arangodb/testutils/test-utils');
const internal = require('internal');
const platform = internal.platform;

const BLUE = internal.COLORS.COLOR_BLUE;
const CYAN = internal.COLORS.COLOR_CYAN;
const GREEN = internal.COLORS.COLOR_GREEN;
const RED = internal.COLORS.COLOR_RED;
const RESET = internal.COLORS.COLOR_RESET;
const YELLOW = internal.COLORS.COLOR_YELLOW;

let functionsDocumentation = {
  'all': 'run all tests (marked with [x])',
  'find': 'searches all testcases, and eventually filters them by `--test`, ' +
    'will dump testcases associated to testsuites.',
  'auto': 'uses find; if the testsuite for the testcase is located, ' +
    'runs the suite with the filter applied'
};

let optionsDocumentation = [
  '',

  ' The following properties of `options` are defined:',
  '',
  '   - `testOutput`: set the output directory for testresults, defaults to `out`',
  '   - `force`: if set to true the tests are continued even if one fails',
  '',
  "   - `skipLogAnalysis`: don't try to crawl the server logs",
  '   - `skipMemoryIntense`: tests using lots of resources will be skipped.',
  '   - `skipNightly`: omit the nightly tests',
  '   - `skipRanges`: if set to true the ranges tests are skipped',
  '   - `skipTimeCritical`: if set to true, time critical tests will be skipped.',
  '   - `skipNondeterministic`: if set, nondeterministic tests are skipped.',
  '   - `skipGrey`: if set, grey tests are skipped.',
  '   - `onlyGrey`: if set, only grey tests are executed.',
  '   - `testBuckets`: split tests in to buckets and execute on, for example',
  '       10/2 will split into 10 buckets and execute the third bucket.',
  '',
  '   - `onlyNightly`: execute only the nightly tests',
  '   - `loopEternal`: to loop one test over and over.',
  '   - `loopSleepWhen`: sleep every nth iteration',
  '   - `loopSleepSec`: sleep seconds between iterations',
  '   - `sleepBeforeStart` : sleep at tcpdump info - use this dump traffic or attach debugger',
  '   - `sleepBeforeShutdown`: let the system rest before terminating it',
  '',
  '   - `storageEngine`: set to `rocksdb` - defaults to `rocksdb`',
  '',
  '   - `server`: server_url (e.g. tcp://127.0.0.1:8529) for external server',
  '   - `serverRoot`: directory where data/ points into the db server. Use in',
  '                   conjunction with `server`.',
  '   - `cluster`: if set to true the tests are run with the coordinator',
  '     of a small local cluster',
  '   - `arangosearch`: if set to true enable the ArangoSearch-related tests',
  '   - `minPort`: minimum port number to use',
  '   - `maxPort`: maximum port number to use',
  '   - `forceJson`: don\'t use vpack - for better debugability',
  '   - `vst`: attempt to connect to the SUT via vst',
  '   - `http2`: attempt to connect to the SUT via http2',
  '   - `dbServers`: number of DB-Servers to use',
  '   - `coordinators`: number coordinators to use',
  '   - `agency`: if set to true agency tests are done',
  '   - `agencySize`: number of agents in agency',
  '   - `agencySupervision`: run supervision in agency',
  '   - `oneTestTimeout`: how long a single testsuite (.js, .rb)  should run',
  '   - `isAsan`: doubles oneTestTimeot value if set to true (for ASAN-related builds)',
  '   - `memprof`: take snapshots (requries memprof enabled build)',
  '   - `test`: path to single test to execute for "single" test target, ',
  '             or pattern to filter for other suites',
  '   - `cleanup`: if set to false the data files',
  '                and logs are not removed after termination of the test.',
  '',
  '   - `protocol`: the protocol to talk to the server - [tcp (default), ssl, unix]',
  '   - `sniff`: if we should try to launch tcpdump / windump for a testrun',
  '              false / true / sudo',
  '   - `sniffDevice`: the device tcpdump / tshark should use',
  '   - `sniffProgram`: specify your own programm',
  '   - `sniffAgency`: when sniffing cluster, sniff agency traffic too? (true)',
  '   - `sniffDBServers`: when sniffing cluster, sniff dbserver traffic too? (true)',
  '',
  '   - `build`: the directory containing the binaries',
  '   - `buildType`: Windows build type (Debug, Release), leave empty on linux',
  '   - `configDir`: the directory containing the config files, defaults to',
  '                  etc/testing',
  '   - `writeXmlReport`:  Write junit xml report files',
  '   - `dumpAgencyOnError`: if we should create an agency dump if an error occurs',
  '   - `prefix`:    prefix for the tests in the xml reports',
  '',
  '   - `disableClusterMonitor`: if set to false, an arangosh is started that will send',
  '                              keepalive requests to all cluster instances, and report on error',
  '   - `disableMonitor`: if set to true on windows, procdump will not be attached.',
  '   - `rr`: if set to true arangod instances are run with rr',
  '   - `exceptionFilter`: on windows you can use this to abort tests on specific exceptions',
  '                        i.e. `bad_cast` to abort on throwing of std::bad_cast',
  '                        or a coma separated list for multiple exceptions; ',
  '                        filtering by asterisk is possible',
  '   - `exceptionCount`: how many exceptions should procdump be able to capture?',
  '   - `coreCheck`: if set to true, we will attempt to locate a coredump to ',
  '                  produce a backtrace in the event of a crash',
  '',
  '   - `sanitizer`: if set the programs are run with enabled sanitizer',
  '     and need longer timeouts',
  '',
  '   - `activefailover` starts active failover single server setup (active/passive)',
  '   -  `singles` the number of servers in an active failover test, defaults to 2',
  '',
  '   - `valgrind`: if set the programs are run with the valgrind',
  '     memory checker; should point to the valgrind executable',
  '   - `valgrindFileBase`: string to prepend to the report filename',
  '   - `valgrindArgs`: commandline parameters to add to valgrind',
  '   - valgrindHosts  - configure which clustercomponents to run using valgrind',
  '        Coordinator - flag to run Coordinator with valgrind',
  '        DBServer    - flag to run DBServers with valgrind',
  '',
  '   - `extraArgs`: list of extra commandline arguments to add to arangod',
  '',
  '   - `testFailureText`: filename of the testsummary file',
  '   - `crashAnalysisText`: output of debugger in case of crash',
  '   - `getSockStat`: on linux collect socket stats before shutdown',
  '   - `verbose`: if set to true, be more verbose',
  '   - `extremeVerbosity`: if set to true, then there will be more test run',
  '     output, especially for cluster tests.',
  '   - `testCase`: filter a jsunity testsuite for one special test case',
  '   - `failed`: if set to true, re-runs only those tests that failed in the',
  '     previous test run. The information which tests previously failed is taken',
  '     from the "UNITTEST_RESULT.json" (if available).',
  '   - `encryptionAtRest`: enable on disk encryption, enterprise only',
  ''
];

const optionsDefaults = {
  'dumpAgencyOnError': false,
  'agencySize': 3,
  'agencyWaitForSync': false,
  'agencySupervision': true,
  'build': '',
  'buildType': (platform.substr(0, 3) === 'win') ? 'RelWithDebInfo':'',
  'cleanup': true,
  'cluster': false,
  'concurrency': 3,
  'configDir': 'etc/testing',
  'coordinators': 1,
  'coreCheck': false,
  'coreDirectory': '/var/tmp',
  'dbServers': 2,
  'duration': 10,
  'encryptionAtRest': false,
  'extraArgs': {},
  'extremeVerbosity': false,
  'force': true,
  'forceJson': false,
  'getSockStat': false,
  'arangosearch':true,
  'loopEternal': false,
  'loopSleepSec': 1,
  'loopSleepWhen': 1,
  'minPort': 1024,
  'maxPort': 32768,
  'memprof': false,
  'onlyNightly': false,
  'password': '',
  'protocol': 'tcp',
  'replication': false,
  'rr': false,
  'exceptionFilter': null,
  'exceptionCount': 1,
  'sanitizer': false,
  'activefailover': false,
  'singles': 2,
  'sniff': false,
  'sniffAgency': true,
  'sniffDBServers': true,
  'sniffDevice': undefined,
  'sniffProgram': undefined,
  'skipLogAnalysis': true,
  'skipMemoryIntense': false,
  'skipNightly': true,
  'skipNondeterministic': false,
  'skipGrey': false,
  'onlyGrey': false,
  'oneTestTimeout': 15 * 60,
  'isAsan': (
      global.ARANGODB_CLIENT_VERSION(true).asan === 'true' ||
      global.ARANGODB_CLIENT_VERSION(true).tsan === 'true'),
  'skipTimeCritical': false,
  'storageEngine': 'rocksdb',
  'test': undefined,
  'testBuckets': undefined,
  'testOutputDirectory': 'out',
  'useReconnect': true,
  'username': 'root',
  'valgrind': false,
  'valgrindFileBase': '',
  'valgrindArgs': {},
  'valgrindHosts': false,
  'verbose': false,
  'vst': false,
  'http2': false,
  'walFlushTimeout': 30000,
  'writeXmlReport': false,
  'testFailureText': 'testfailures.txt',
  'crashAnalysisText': 'testfailures.txt',
  'testCase': undefined,
  'disableMonitor': false,
  'disableClusterMonitor': true,
  'sleepBeforeStart' : 0,
  'sleepBeforeShutdown' : 0,
  'failed': false,
};

let globalStatus = true;

let allTests = [];
let testFuncs = {};

// //////////////////////////////////////////////////////////////////////////////
// / @brief print usage information
// //////////////////////////////////////////////////////////////////////////////

function printUsage () {
  print();
  print('Usage: UnitTest([which, ...], options)');
  print();
  print('       where "which" is one of:\n');

  for (let i in testFuncs) {
    if (testFuncs.hasOwnProperty(i)) {
      let oneFunctionDocumentation;

      if (functionsDocumentation.hasOwnProperty(i)) {
        oneFunctionDocumentation = ' - ' + functionsDocumentation[i];
      } else {
        oneFunctionDocumentation = '';
      }

      let checkAll;

      if (allTests.indexOf(i) !== -1) {
        checkAll = '[x]';
      } else {
        checkAll = '   ';
      }

      print('    ' + checkAll + ' ' + i + ' ' + oneFunctionDocumentation);
    }
  }

  for (let i in optionsDocumentation) {
    if (optionsDocumentation.hasOwnProperty(i)) {
      print(optionsDocumentation[i]);
    }
  }
}

let allTestPaths = {};

function findTestCases(options) {
  let filterTestcases = (options.hasOwnProperty('test') && (typeof (options.test) !== 'undefined'));
  let found = !filterTestcases;
  let allTestFiles = {};
  for (let testSuiteName in allTestPaths) {
    var myList = [];
    var _opts = _.clone(options);
    _opts.extremeVerbosity = 'silence';
    let files = tu.scanTestPaths(allTestPaths[testSuiteName], _opts);
    if (options.hasOwnProperty('test') && (typeof (options.test) !== 'undefined')) {
      for (let j = 0; j < files.length; j++) {
        let foo = {};
        if (tu.filterTestcaseByOptions(files[j], options, foo)) {
          myList.push(files[j]);
          found = true;
        }
      }
    } else {
      myList = myList.concat(files);
    }

    if (!filterTestcases || (myList.length > 0)) {
      allTestFiles[testSuiteName] = myList;
    }
  }
  return [found, allTestFiles];
}

function isClusterTest(options) {
  let rc = false;
  if (options.hasOwnProperty('test') && (typeof (options.test) !== 'undefined')) {
    if (typeof (options.test) === 'string') {
      rc = (options.test.search('-cluster') >= 0);
    } else {
      options.test.forEach(
        filter => {
          if (filter.search('-cluster') >= 0) {
            rc = true;
          }
        }
      );
    }
  }
  return rc;
}

function findTest(options) {
  if (isClusterTest(options)) {
    options.cluster = true;
  }
  let rc = findTestCases(options);
  if (rc[0]) {
    print(rc[1]);
    return {
      findTest: {
        status: true,
        total: 1,
        message: 'we have found a test. see above.',
        duration: 0,
        failed: [],
        found: {
          status: true,
          duration: 0,
          message: 'we have found a test.'
        }
      }
    };
  } else {
    return {
      findTest: {
        status: false,
        total: 1,
        failed: 1,
        message: 'we haven\'t found a test.',
        duration: 0,
        found: {
          status: false,
          duration: 0,
          message: 'we haven\'t found a test.'
        }
      }
    };
  }
}

function autoTest(options) {
  if (!options.hasOwnProperty('test') || (typeof (options.test) === 'undefined')) {
    return {
      findTest: {
        status: false,
        total: 1,
        failed: 1,
        message: 'you must specify a --test filter.',
        duration: 0,
        found: {
          status: false,
          duration: 0,
          message: 'you must specify a --test filter.'
        }
      }
    };
  }
  if (isClusterTest(options)) {
    options.cluster = true;
  }
  let rc = findTestCases(options);
  if (rc[0]) {
    let testSuites = Object.keys(rc[1]);
    return iterateTests(testSuites, options, true);
  } else {
    return {
      findTest: {
        status: false,
        total: 1,
        failed: 1,
        message: 'we haven\'t found a test.',
        duration: 0,
        found: {
          status: false,
          duration: 0,
          message: 'we haven\'t found a test.'
        }
      }
    };
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief load the available testsuites
// //////////////////////////////////////////////////////////////////////////////

function loadTestSuites () {
  let testSuites = _.filter(fs.list(fs.join(__dirname, '..', 'testsuites')),
                            function (p) {
                              return (p.substr(-3) === '.js');
                            }).sort();

  for (let j = 0; j < testSuites.length; j++) {
    try {
      require('@arangodb/testsuites/' + testSuites[j]).setup(testFuncs,
                                                             allTests,
                                                             optionsDefaults,
                                                             functionsDocumentation,
                                                             optionsDocumentation,
                                                             allTestPaths);
    } catch (x) {
      print('failed to load module ' + testSuites[j]);
      throw x;
    }
  }
  testFuncs['all'] = allTests;
  testFuncs['find'] = findTest;
  testFuncs['auto'] = autoTest;
}

function translateTestList(cases) {
  let caselist = [];
  const expandWildcard = ( name ) => {
    if (!name.endsWith('*')) {
      return name;
    }
    const prefix = name.substring(0, name.length - 1);
    return allTests.filter( ( s ) => s.startsWith(prefix) ).join(',');
  };

  for (let n = 0; n < cases.length; ++n) {
    let splitted = expandWildcard(cases[n]).split(/[,;|]/);
    for (let m = 0; m < splitted.length; ++m) {
      let which = splitted[m];

      if (testFuncs.hasOwnProperty(which)) {
        caselist.push(which);
      } else {
        print('Unknown test "' + which + '"\nKnown tests are: ' + Object.keys(testFuncs).sort().join(', '));
        throw new Error("USAGE ERROR");
      }
    }
  }
  // Expand meta tests like ldap, all
  caselist = (function() {
    let flattened = [];
    for (let n = 0; n < caselist.length; ++n) {
      let w = testFuncs[caselist[n]];
      if (Array.isArray(w)) {
        w.forEach(function(sub) { flattened.push(sub); });
      } else {
        flattened.push(caselist[n]);
      }
    }
    return flattened;
  })();
  if (cases === undefined || cases.length === 0) {
    printUsage();
    
    print('\nFATAL: "which" is undefined\n');
    throw new Error("USAGE ERROR");
  }
  return caselist;
}

function iterateTests(cases, options) {
  // tests to run
  let caselist = [];

  let results = {};
  let cleanup = true;

  if (options.failed) {
    // we are applying the failed filter -> only consider cases with failed tests
    cases = _.filter(cases, c => options.failed.hasOwnProperty(c));
  }
  caselist = translateTestList(cases);
  // running all tests
  for (let n = 0; n < caselist.length; ++n) {
    const currentTest = caselist[n];
    var localOptions = _.cloneDeep(options);
    if (localOptions.failed) {
      localOptions.failed = localOptions.failed[currentTest];
    }
    let printTestName = currentTest;
    if (options.testBuckets) {
      printTestName += " - " + options.testBuckets;
    }
    print(YELLOW + '================================================================================');
    print('Executing test', printTestName);
    print('================================================================================\n' + RESET);

    if (localOptions.verbose) {
      print(CYAN + 'with options:', localOptions, RESET);
    }

    let result;
    let status = true;
    let shutdownSuccess = true;

    result = testFuncs[currentTest](localOptions);
    // grrr...normalize structure
    delete result.status;
    delete result.failed;
    delete result.crashed;
    if (result.hasOwnProperty('shutdown')) {
      shutdownSuccess = result['shutdown'];
      delete result.shutdown;
    }

    status = rp.gatherStatus(result);
    let failed = rp.gatherFailed(result);
    if (!status) {
      globalStatus = false;
    }
    result.failed = failed;
    result.status = status;
    results[currentTest] = result;

    if (status && localOptions.cleanup && shutdownSuccess ) {
      pu.cleanupLastDirectory(localOptions);
    } else {
      cleanup = false;
    }
    pu.aggregateFatalErrors(currentTest);
  }

  results.status = globalStatus;
  results.crashed = pu.serverCrashed;

  if (options.server === undefined) {
    if (cleanup && globalStatus && !pu.serverCrashed) {
      pu.cleanupDBDirectories(options);
    } else {
      print('not cleaning up as some tests weren\'t successful:\n' +
            pu.getCleanupDBDirectories() + " " +
            cleanup + ' - ' + globalStatus + ' - ' + pu.serverCrashed + "\n");
    }
  } else {
    print("not cleaning up since we didn't start the server ourselves\n");
  }

  if (options.extremeVerbosity === true) {
    rp.yamlDumpResults(options, results);
  }
  return results;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief framework to perform unittests
// /
// / This function gets one or two arguments, the first describes which tests
// / to perform and the second is an options object. For `which` the following
// / values are allowed:
// /  Empty will give you a complete list.
// //////////////////////////////////////////////////////////////////////////////

function unitTest (cases, options) {
  if (typeof options !== 'object') {
    options = {};
  }
  loadTestSuites(options);
  // testsuites may register more defaults...
  _.defaults(options, optionsDefaults);
  if (options.failed ||
      (Array.isArray(options.commandSwitches) && options.commandSwitches.includes("failed"))) {
    options.failed = rp.getFailedTestCases(options);
  }

  try {
    pu.setupBinaries(options.build, options.buildType, options.configDir);
  }
  catch (err) {
    print(err);
    return {
      status: false,
      crashed: true,
      ALL: [{
        status: false,
        failed: 1,
        message: err.message
      }]
    };
  }

  arango.forceJson(options.forceJson);

  if ((cases.length === 1) && cases[0] === 'auto') {
    return autoTest(options);
  } else {
    return iterateTests(cases, options);
  }
}

// /////////////////////////////////////////////////////////////////////////////
// exports
// /////////////////////////////////////////////////////////////////////////////
exports.optionsDefaults = optionsDefaults;
exports.unitTest = unitTest;

exports.testFuncs = testFuncs;
