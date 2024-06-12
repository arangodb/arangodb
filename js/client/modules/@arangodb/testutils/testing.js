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
// / @author Max Neunhoeffer
// / @author Wilfried Goesgnes
// //////////////////////////////////////////////////////////////////////////////

const _ = require('lodash');
const fs = require('fs');

const pu = require('@arangodb/testutils/process-utils');
const rp = require('@arangodb/testutils/result-processing');
const cu = require('@arangodb/testutils/crash-utils');
const tu = require('@arangodb/testutils/test-utils');
const {versionHas, flushInstanceInfo} = require("@arangodb/test-helper");
const internal = require('internal');
const platform = internal.platform;

const BLUE = internal.COLORS.COLOR_BLUE;
const CYAN = internal.COLORS.COLOR_CYAN;
const GREEN = internal.COLORS.COLOR_GREEN;
const RED = internal.COLORS.COLOR_RED;
const RESET = internal.COLORS.COLOR_RESET;
const YELLOW = internal.COLORS.COLOR_YELLOW;

let functionsDocumentation = {
  'find': 'searches all testcases, and eventually filters them by `--test`, ' +
    'will dump testcases associated to testsuites.',
  'auto': 'uses find; if the testsuite for the testcase is located, ' +
    'runs the suite with the filter applied'
};

let optionsDocumentation = [
  '',

  ' The following properties of `options` are defined:',
  '   - `force`: if set to true the tests are continued even if one fails',
  '   - `setInterruptable`: register special break handler',
  '',
  '   - `forceJson`: don\'t use vpack - for better debugability',
  '   - `protocol`: the protocol to talk to the server - [tcp (default), ssl, unix]',
  '   - `vst`: attempt to connect to the SUT via vst',
  '   - `http2`: attempt to connect to the SUT via http2',
  '   - `oneTestTimeout`: how long a single js testsuite  should run',
  '   - `cleanup`: if set to false the data files',
  '                and logs are not removed after termination of the test.',
  '',
  '   - `verbose`: if set to true, be more verbose',
  '   - `noStartStopLogs`: if set to true, suppress startup and shutdown messages printed by process manager. Overridden by `extremeVerbosity`',
  '   - `extremeVerbosity`: if set to true, then there will be more test run',
  '     output, especially SUT process management etc.',
  '   - `failed`: if set to true, re-runs only those tests that failed in the',
  '     previous test run. The information which tests previously failed is taken',
  '     from the "UNITTEST_RESULT.json" (if available).',
  '   - `optionsJson`: all of the above, as json list for mutliple suite launches',
  ''
];

const isCoverage = versionHas('coverage');
const isInstrumented = versionHas('asan') || versionHas('tsan') || versionHas('coverage');
const optionsDefaults = {
  'cleanup': true,
  'concurrency': 3,
  'duration': 10,
  'extremeVerbosity': false,
  'force': true,
  'forceJson': false,
  'password': '',
  'protocol': 'tcp',
  'replication': false,
  'setInterruptable': ! internal.isATTy(),
  'oneTestTimeout': (isInstrumented? 25 : 15) * 60,
  'isCov': isCoverage,
  'isInstrumented': isInstrumented,
  'useReconnect': true,
  'username': 'root',
  'verbose': false,
  'noStartStopLogs': internal.isATTy(),
  'vst': false,
  'http2': false,
  'failed': false,
  'optionsJson': null,
};

let globalStatus = true;

let allTests = [];
let testFuncs = {};
let optionHandlers = [];

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

      print(`     ${i} ${oneFunctionDocumentation}`);
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
                                                             optionsDefaults,
                                                             functionsDocumentation,
                                                             optionsDocumentation,
                                                             allTestPaths);
    } catch (x) {
      print('failed to load module ' + testSuites[j]);
      throw x;
    }
  }
  allTests = Object.keys(testFuncs);
  testFuncs['find'] = findTest;
  testFuncs['auto'] = autoTest;
}

let MODULES_LOADED=false;
function loadModuleOptions () {
  if (MODULES_LOADED) {
    return false;
  }
  MODULES_LOADED = true;
  const nonModules = ['clusterstats.js',
                      'joinHelper.js',
                      'testing.js',
                      'block-cache-test-helper.js',
                      'cluster-test-helper.js',
                      'user-helper.js',
                      'unittest.js'
                     ];
  let testModules = _.filter(fs.list(__dirname),
                             function (p) {
                               return (
                                 p.endsWith('.js') && !(
                                   (p.indexOf('aql-') !== -1) ||
                                     (nonModules.find(m => { return m === p;}) === p)
                                 )
                               );
                             }).sort();
  for (let j = 0; j < testModules.length; j++) {
    try {
      let m = require('@arangodb/testutils/' + testModules[j]);
      if (m.hasOwnProperty('registerOptions')){
        m.registerOptions(optionsDefaults, optionsDocumentation, optionHandlers);
      }
    } catch (x) {
      print('failed to load module ' + testModules[j]);
      throw x;
    }
  }
  optionsDocumentation.push(' testsuite specific options:');
}

function translateTestList(cases, options) {
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
        if (fs.exists(which)) {
          options.test = which;
          return translateTestList(['auto'], options);
        }
        print('Unknown test "' + which + '"\nKnown tests are: ' + Object.keys(testFuncs).sort().join(', '));
        throw new Error("USAGE ERROR");
      }
    }
  }
  // Expand meta tests like "all"
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

  if (options.extremeVerbosity === true) {
    internal.logLevel('V8=debug');
  }
  if (options.failed) {
    // we are applying the failed filter -> only consider cases with failed tests
    cases = _.filter(cases, c => options.failed.hasOwnProperty(c));
  }
  caselist = translateTestList(cases, options);
  let optionsList = [];
  if (options.optionsJson != null) {
    optionsList = JSON.parse(options.optionsJson);
    if (optionsList.length !== caselist.length) {
      throw new Error("optionsJson must have one entry per suite!");
    }
  }
  // running all tests
  for (let n = 0; n < caselist.length; ++n) {
    // required, because each different test suite may operate with a different set of servers!
    flushInstanceInfo();

    const currentTest = caselist[n];
    var localOptions = _.cloneDeep(options);
    if (localOptions.failed) {
      localOptions.failed = localOptions.failed[currentTest];
    }
    if (optionsList.length !== 0) {
      localOptions = _.defaults(optionsList[n], localOptions);
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

    status = rp.gatherStatus(result) && shutdownSuccess;
    let failed = rp.gatherFailed(result);
    if (!status) {
      globalStatus = false;
    }
    result.failed = failed;
    result.status = status;
    results[currentTest] = result;
  }

  results.status = globalStatus;
  results.crashed = pu.serverCrashed;

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
  loadModuleOptions();
  loadTestSuites();

  // testsuites may register more defaults...
  _.defaults(options, optionsDefaults);

  if (options.extremeVerbosity) {
    print(JSON.stringify(options));
  }
  if (options.failed ||
      (Array.isArray(options.commandSwitches) && options.commandSwitches.includes("failed"))) {
    options.failed = rp.getFailedTestCases(options);
  }
  if (options.setInterruptable) {
    internal.SetSignalToImmediateDeadline();
  }
  optionHandlers.forEach(h => h(options));
  try {
    pu.setupBinaries(options);
  }
  catch (err) {
    print(err);
    return {
      status: false,
      crashed: true,
      ALL: [{
        status: false,
        failed: 1,
        message: `${err.message} ; ${err.stack}`
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

function dumpCompletions() {
  loadTestSuites();
  const result = {};
  result.options = Object.keys(optionsDefaults).map(o => '--' + o).sort();
  result.suites = allTests.concat('auto', 'find').sort();
  print(JSON.stringify(result));
  return 0;
}

// /////////////////////////////////////////////////////////////////////////////
// exports
// /////////////////////////////////////////////////////////////////////////////
Object.defineProperty(exports, 'optionsDefaults', {
  get: () => {
    loadModuleOptions();
    return optionsDefaults;
  }
});
exports.unitTest = unitTest;

exports.testFuncs = testFuncs;
exports.dumpCompletions = dumpCompletions;
