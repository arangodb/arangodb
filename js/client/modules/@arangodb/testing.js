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

let functionsDocumentation = {
  'all': 'run all tests (marked with [x])'
};

let optionsDocumentation = [
  '',
  ' The following properties of `options` are defined:',
  '',
  '   - `jsonReply`: if set a json is returned which the caller has to ',
  '        present the user',
  '   - `force`: if set to true the tests are continued even if one fails',
  '',
  "   - `skipLogAnalysis`: don't try to crawl the server logs",
  '   - `skipMemoryIntense`: tests using lots of resources will be skipped.',
  '   - `skipNightly`: omit the nightly tests',
  '   - `skipRanges`: if set to true the ranges tests are skipped',
  '   - `skipTimeCritical`: if set to true, time critical tests will be skipped.',
  '   - `skipNondeterministic`: if set, nondeterministic tests are skipped.',
  '   - `testBuckets`: split tests in to buckets and execute on, for example',
  '       10/2 will split into 10 buckets and execute the third bucket.',
  '',
  '   - `onlyNightly`: execute only the nightly tests',
  '   - `loopEternal`: to loop one test over and over.',
  '   - `loopSleepWhen`: sleep every nth iteration',
  '   - `loopSleepSec`: sleep seconds between iterations',
  '',
  '   - `storageEngine`: set to `rocksdb` or `mmfiles` - defaults to `mmfiles`',
  '',
  '   - `server`: server_url (e.g. tcp://127.0.0.1:8529) for external server',
  '   - `serverRoot`: directory where data/ points into the db server. Use in',
  '                   conjunction with `server`.',
  '   - `cluster`: if set to true the tests are run with the coordinator',
  '     of a small local cluster',
  '   - `dbServers`: number of DB-Servers to use',
  '   - `coordinators`: number coordinators to use',
  '   - `agency`: if set to true agency tests are done',
  '   - `agencySize`: number of agents in agency',
  '   - `agencySupervision`: run supervision in agency',
  '   - `test`: path to single test to execute for "single" test target',
  '   - `cleanup`: if set to true (the default), the cluster data files',
  '     and logs are removed after termination of the test.',
  '',
  '   - `build`: the directory containing the binaries',
  '   - `buildType`: Windows build type (Debug, Release), leave empty on linux',
  '   - `configDir`: the directory containing the config files, defaults to',
  '                  etc/testing',
  '   - `writeXml`:  Write junit xml report files',
  '   - `prefix`:    prefix for the tests in the xml reports',
  '',
  '   - `rr`: if set to true arangod instances are run with rr',
  '',
  '   - `sanitizer`: if set the programs are run with enabled sanitizer',
  '     and need longer timeouts',
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
  '   - `verbose`: if set to true, be more verbose',
  '   - `extremeVerbosity`: if set to true, then there will be more test run',
  '     output, especially for cluster tests.',
  ''
];

const optionsDefaults = {
  'agencySize': 3,
  'agencyWaitForSync': false,
  'agencySupervision': true,
  'build': '',
  'buildType': '',
  'cleanup': true,
  'cluster': false,
  'concurrency': 3,
  'configDir': 'etc/testing',
  'coordinators': 1,
  'coreDirectory': '/var/tmp',
  'dbServers': 2,
  'duration': 10,
  'extraArgs': {},
  'extremeVerbosity': false,
  'force': true,
  'jsonReply': false,
  'loopEternal': false,
  'loopSleepSec': 1,
  'loopSleepWhen': 1,
  'minPort': 1024,
  'maxPort': 32768,
  'mochaGrep': undefined,
  'onlyNightly': false,
  'password': '',
  'replication': false,
  'rr': false,
  'sanitizer': false,
  'skipLogAnalysis': true,
  'skipMemoryIntense': false,
  'skipNightly': true,
  'skipNondeterministic': false,
  'skipTimeCritical': false,
  'storageEngine': 'mmfiles',
  'test': undefined,
  'testBuckets': undefined,
  'username': 'root',
  'valgrind': false,
  'valgrindFileBase': '',
  'valgrindArgs': {},
  'valgrindHosts': false,
  'verbose': false,
  'walFlushTimeout': 30000,
  'writeXmlReport': true,
  'testFailureText': 'testfailures.txt'
};

const _ = require('lodash');
const fs = require('fs');
const yaml = require('js-yaml');

const pu = require('@arangodb/process-utils');
const cu = require('@arangodb/crash-utils');

const BLUE = require('internal').COLORS.COLOR_BLUE;
const CYAN = require('internal').COLORS.COLOR_CYAN;
const GREEN = require('internal').COLORS.COLOR_GREEN;
const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
const YELLOW = require('internal').COLORS.COLOR_YELLOW;

// //////////////////////////////////////////////////////////////////////////////
// / @brief test functions for all
// //////////////////////////////////////////////////////////////////////////////

let allTests = [
];

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: all
// //////////////////////////////////////////////////////////////////////////////

let testFuncs = {
  'all': function () {}
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief internal members of the results
// //////////////////////////////////////////////////////////////////////////////

const internalMembers = [
  'code',
  'error',
  'status',
  'duration',
  'failed',
  'total',
  'crashed',
  'ok',
  'message',
  'suiteName'
];

// //////////////////////////////////////////////////////////////////////////////
// / @brief pretty prints the result
// //////////////////////////////////////////////////////////////////////////////

function testCaseMessage (test) {
  if (typeof test.message === 'object' && test.message.hasOwnProperty('body')) {
    return test.message.body;
  } else {
    return test.message;
  }
}

function unitTestPrettyPrintResults (r, testOutputDirectory, options) {
  function skipInternalMember (r, a) {
    return !r.hasOwnProperty(a) || internalMembers.indexOf(a) !== -1;
  }
  print(BLUE + '================================================================================');
  print('TEST RESULTS');
  print('================================================================================\n' + RESET);

  let failedSuite = 0;
  let failedTests = 0;

  let onlyFailedMessages = '';
  let failedMessages = '';
  let SuccessMessages = '';
  try {
    /* jshint forin: false */
    for (let testrunName in r) {
      if (skipInternalMember(r, testrunName)) {
        continue;
      }

      let testrun = r[testrunName];

      let successCases = {};
      let failedCases = {};
      let isSuccess = true;

      for (let testName in testrun) {
        if (skipInternalMember(testrun, testName)) {
          continue;
        }

        let test = testrun[testName];

        if (test.status) {
          successCases[testName] = test;
        } else {
          isSuccess = false;
          ++failedSuite;

          if (test.hasOwnProperty('message')) {
            ++failedTests;
            failedCases[testName] = {
              test: testCaseMessage(test)
            };
          } else {
            let fails = failedCases[testName] = {};

            for (let oneName in test) {
              if (skipInternalMember(test, oneName)) {
                continue;
              }

              let oneTest = test[oneName];

              if (!oneTest.status) {
                ++failedTests;
                fails[oneName] = testCaseMessage(oneTest);
              }
            }
          }
        }
      }

      if (isSuccess) {
        SuccessMessages += '* Test "' + testrunName + '"\n';

        for (let name in successCases) {
          if (!successCases.hasOwnProperty(name)) {
            continue;
          }

          let details = successCases[name];

          if (details.skipped) {
            SuccessMessages += YELLOW + '    [SKIPPED] ' + name + RESET + '\n';
          } else {
            SuccessMessages += GREEN + '    [SUCCESS] ' + name + RESET + '\n';
          }
        }
      } else {
        let m = '* Test "' + testrunName + '"\n';
        onlyFailedMessages += m;
        failedMessages += m;

        for (let name in successCases) {
          if (!successCases.hasOwnProperty(name)) {
            continue;
          }

          let details = successCases[name];

          if (details.skipped) {
            failedMessages += YELLOW + '    [SKIPPED] ' + name + RESET + '\n';
            onlyFailedMessages += '    [SKIPPED] ' + name + '\n';
          } else {
            failedMessages += GREEN + '    [SUCCESS] ' + name + RESET + '\n';
          }
        }

        for (let name in failedCases) {
          if (!failedCases.hasOwnProperty(name)) {
            continue;
          }

          failedMessages += RED + '    [FAILED]  ' + name + RESET + '\n\n';
          onlyFailedMessages += '    [FAILED]  ' + name + '\n\n';

          let details = failedCases[name];

          let count = 0;
          for (let one in details) {
            if (!details.hasOwnProperty(one)) {
              continue;
            }

            if (count > 0) {
              failedMessages += '\n';
              onlyFailedMessages += '\n';
            }
            failedMessages += RED + '      "' + one + '" failed: ' + details[one] + RESET + '\n';
            onlyFailedMessages += '      "' + one + '" failed: ' + details[one] + '\n';
            count++;
          }
        }
      }
    }
    print(SuccessMessages);
    print(failedMessages);
    /* jshint forin: true */

    let color = (!r.crashed && r.status === true) ? GREEN : RED;
    let crashText = '';
    let crashedText = '';
    if (r.crashed === true) {
      crashedText = ' BUT! - We had at least one unclean shutdown or crash during the testrun.';
      crashText = RED + crashedText + RESET;
    }
    print('\n' + color + '* Overall state: ' + ((r.status === true) ? 'Success' : 'Fail') + RESET + crashText);

    let failText = '';
    if (r.status !== true) {
      failText = '   Suites failed: ' + failedSuite + ' Tests Failed: ' + failedTests;
      print(color + failText + RESET);
    }

    failedMessages = onlyFailedMessages + crashedText + cu.GDB_OUTPUT + failText + '\n';
    fs.write(testOutputDirectory + options.testFailureText, failedMessages);
  } catch (x) {
    print('exception caught while pretty printing result: ');
    print(x.message);
    print(JSON.stringify(r));
  }
}

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

// //////////////////////////////////////////////////////////////////////////////
// / @brief load the available testsuites
// //////////////////////////////////////////////////////////////////////////////
function loadTestSuites () {
  let testSuites = _.filter(fs.list(fs.join(__dirname, 'testsuites')),
                            function (p) {
                              return (p.substr(-3) === '.js');
                            })
      .map(function (x) {
        return x;
      }).sort();

  for (let j = 0; j < testSuites.length; j++) {
    try {
      require('@arangodb/testsuites/' + testSuites[j]).setup(testFuncs,
                                                             allTests,
                                                             optionsDefaults,
                                                             functionsDocumentation,
                                                             optionsDocumentation);
    } catch (x) {
      print('failed to load module ' + testSuites[j]);
      throw x;
    }
  }
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
  loadTestSuites();
  _.defaults(options, optionsDefaults);

  if (cases === undefined || cases.length === 0) {
    printUsage();

    print('FATAL: "which" is undefined\n');

    return {
      status: false,
      crashed: false
    };
  }

  pu.setupBinaries(options.build, options.buildType, options.configDir);
  const jsonReply = options.jsonReply;
  delete options.jsonReply;

  // tests to run
  let caselist = [];

  for (let n = 0; n < cases.length; ++n) {
    let splitted = cases[n].split(/[,;\.|]/);

    for (let m = 0; m < splitted.length; ++m) {
      let which = splitted[m];

      if (which === 'all') {
        caselist = caselist.concat(allTests);
      } else if (testFuncs.hasOwnProperty(which)) {
        caselist.push(which);
      } else {
        let line = 'Unknown test "' + which + '"\nKnown tests are: ';
        let sep = '';

        Object.keys(testFuncs).map(function (key) {
          line += sep + key;
          sep = ', ';
        });

        print(line);

        return {
          status: false
        };
      }
    }
  }

  let globalStatus = true;
  let results = {};
  let cleanup = true;

  // running all tests
  for (let n = 0; n < caselist.length; ++n) {
    const currentTest = caselist[n];
    var localOptions = _.cloneDeep(options);

    print(BLUE + '================================================================================');
    print('Executing test', currentTest);
    print('================================================================================\n' + RESET);

    if (localOptions.verbose) {
      print(CYAN + 'with options:', localOptions, RESET);
    }

    let result = testFuncs[currentTest](localOptions);
    // grrr...normalize structure
    delete result.status;
    delete result.failed;

    let status = Object.values(result).every(testCase => testCase.status === true);
    let failed = Object.values(result).reduce((prev, testCase) => prev + !testCase.status, 0);
    if (!status) {
      globalStatus = false;
    }
    result.failed = failed;
    result.status = status;
    results[currentTest] = result;

    if (status && localOptions.cleanup) {
      pu.cleanupLastDirectory(localOptions);
    }
    else {
      cleanup = false;
    }
  }

  results.status = globalStatus;
  results.crashed = pu.serverCrashed;

  if (options.server === undefined) {
    if (cleanup && globalStatus && !pu.serverCrashed) {
      pu.cleanupDBDirectories(options);
    } else {
      print('not cleaning up as some tests weren\'t successful:\n' +
            pu.getCleanupDBDirectories());
    }
  } else {
    print("not cleaning up since we didn't start the server ourselves\n");
  }

  if (options.extremeVerbosity === true) {
    try {
      print(yaml.safeDump(JSON.parse(JSON.stringify(results))));
    } catch (err) {
      print(RED + 'cannot dump results: ' + String(err) + RESET);
      print(RED + require('internal').inspect(results) + RESET);
    }
  }

  if (jsonReply === true) {
    return results;
  } else {
    return globalStatus;
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief exports
// //////////////////////////////////////////////////////////////////////////////

exports.unitTest = unitTest;

exports.internalMembers = internalMembers;
exports.testFuncs = testFuncs;
exports.unitTestPrettyPrintResults = unitTestPrettyPrintResults;

