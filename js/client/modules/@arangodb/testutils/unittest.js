/* jshint globalstrict:false, unused:false */
/* global print, start_pretty_print, ARGUMENTS */

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
// / @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const _ = require('lodash');
const internal = require('internal');
const rp = require('@arangodb/testutils/result-processing');
const cu = require('@arangodb/testutils/crash-utils');

const testing = require('@arangodb/testutils/testing');
const unitTest = testing.unitTest;
const optionsDefaults = testing.optionsDefaults;
const makeDirectoryRecursive = require('fs').makeDirectoryRecursive;
const killRemainingProcesses = require('@arangodb/testutils/process-utils').killRemainingProcesses;
const inspect = internal.inspect;

// //////////////////////////////////////////////////////////////////////////////
//  @brief runs the test using the test facilities in js/client/modules/@arangodb
// //////////////////////////////////////////////////////////////////////////////

function main (argv) {
  // set deterministic locale value for all tests. 
  // otherwise tests that depend on sort order/collation may
  // behave differently on different platforms
  internal.env.LC_ALL = "en_US.UTF-8";

  if (argv.length >= 1 && argv[0] === '--dump-completions') {
    return testing.dumpCompletions();
  }

  start_pretty_print();

  let testSuites = []; // e.g all, http_server, recovery, ...
  let options = {};

  while (argv.length >= 1) {
    if (argv[0].slice(0, 1) === '-') { // break parsing if we hit some -option
      break;
    }

    testSuites.push(argv[0]); // append first arg to test suits
    argv = argv.slice(1);    // and remove first arg (c++:pop_front/bash:shift)
  }

  if (argv.length >= 1) {
    try {
      options = internal.parseArgv(argv, 0);
    } catch (x) {
      print('failed to parse the options: ' + x.message + '\n' + String(x.stack));
      print('argv: ', argv);
      throw x;
    }
  }

  if (options.hasOwnProperty('testOutput')) {
    options.testOutputDirectory = options.testOutput + '/';
  }
  _.defaults(options, optionsDefaults);

  // create output directory
  try {
    makeDirectoryRecursive(options.testOutputDirectory);
  } catch (x) {
    print("failed to create test directory - " + x.message);
    throw x;
  }

  try {
    // safeguard: mark test as failed if we die for some reason
    rp.writeDefaultReports(options, testSuites);
  } catch (x) {
    print('failed to write default test result: ' + x.message);
    throw x;
  }

  let result = {};
  try {
    result = unitTest(testSuites, options);
  } catch (x) {
    if (x.message === "USAGE ERROR") {
      throw x;
    }
    print('caught exception during test execution!');

    if (x.message !== undefined) {
      print(x.message);
    }

    if (x.stack !== undefined) {
      print(x.stack);
    } else {
      print(x);
    }

    print(JSON.stringify(result));
    killRemainingProcesses(result);
    print(x.stack);
    throw x;
  }

  _.defaults(result, {
    status: false,
    crashed: true
  });

  killRemainingProcesses(result);

  rp.processCrashReport(result);

  try {
    rp.writeReports(options, result);
  } catch (x) {
    print('failed to write test result: ' + x.message);
  }

  rp.dumpAllResults(options, result);
  if (options.writeXmlReport) {
    try {
      rp.analyze.saveToJunitXML(options, result);
    } catch (x) {
      print('exception while serializing status xml!');
      print(x.message);
      print(x.stack);
      print(inspect(result));
    }
  }

  rp.analyze.unitTestPrettyPrintResults(options, result);
  return result.status;
}

let result = main(ARGUMENTS);
if (!result) {
  // force an error in the console
  process.exit(1);
}

