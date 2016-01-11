'use strict';

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDB Mocha integration
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2016 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Alan Plum
/// @author Copyright 2016, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var Module = require('module');
var ArangoError = require('@arangodb').ArangoError;
var errors = require('@arangodb').errors;
var runTests = require('@arangodb/mocha').run;
var internal = require('internal');
var print = internal.print;
var colors = internal.COLORS;

module.exports = function (files, throwOnFail) {
  const results = runTests(run, files, 'suite');
  print();
  logSuite(results);
  logStats(results.stats);
  print();
  if (throwOnFail && results.stats.failures) {
    var failure = findSuiteFailure(results);
    var error = new Error('Test failure');
    error.cause = failure;
    throw error;
  }
  return results.stats.failures === 0;
};

function logStats(stats) {
  print(
    (
      stats.failures
      ? (
        colors.COLOR_RED
        + '[FAIL]'
        + colors.COLOR_RESET
      )
      : (
        colors.COLOR_GREEN
        + '[PASS]'
        + colors.COLOR_RESET
      )
    )
    + ' Completed '
    + colors.COLOR_BOLD_WHITE
    + stats.tests
    + colors.COLOR_RESET
    + ' tests in '
    + colors.COLOR_BOLD_WHITE
    + stats.duration + 'ms'
    + colors.COLOR_RESET
    + ' ( '
    + colors.COLOR_GREEN
    + stats.passes
    + colors.COLOR_RESET
    + ' | '
    + colors.COLOR_RED
    + stats.failures
    + colors.COLOR_RESET
    + ' )'
  );
}

function logSuite(suite, indentLevel) {
  if (!indentLevel) {
    indentLevel = 0;
  }
  if (suite.title) {
    print(
      indent(indentLevel - 1)
      + colors.COLOR_BOLD_WHITE
      + suite.title
      + colors.COLOR_RESET
    );
  }
  for (let test of suite.tests) {
    if (test.result === 'pass') {
      print(
        indent(indentLevel)
        + colors.COLOR_GREEN
        + '[PASS] '
        + test.title
        + colors.COLOR_RESET
      );
    } else {
      print(
        indent(indentLevel)
        + colors.COLOR_RED
        + '[' + test.result.toUpperCase() + '] '
        + test.title
        + colors.COLOR_RESET
      );
      for (let line of test.err.stack.split(/\n/)) {
        print(
          indent(indentLevel + 1)
          + colors.COLOR_RED
          + line
          + colors.COLOR_RESET
        );
      }
    }
  }
  for (let sub of suite.suites) {
    logSuite(sub, indentLevel + 1);
  }
  if (suite.suites.length) {
    print();
  }
}

function indent(level, indentWith) {
  if (!indentWith) {
    indentWith = '    ';
  }
  let padding = '';
  for (let i = 0; i < level; i++) {
    padding += indentWith;
  }
  return padding;
}

function findSuiteFailure(suite) {
  for (let test of suite.tests) {
    if (test.result !== 'pass') {
      return test.err;
    }
  }
  for (let sub of suite.suites) {
    let failure = findSuiteFailure(sub);
    if (failure) {
      return failure;
    }
  }
}

function run(filename, context) {
  var module = new Module(filename);

  if (context) {
    Object.keys(context).forEach(function (key) {
      module.context[key] = context[key];
    });
  }

  try {
    module.load(filename);
    return module.exports;
  } catch(e) {
    var err = new ArangoError({
      errorNum: errors.ERROR_FAILED_TO_EXECUTE_SCRIPT.code,
      errorMessage: errors.ERROR_FAILED_TO_EXECUTE_SCRIPT.message
      + '\nFile: ' + filename
    });
    err.stack = e.stack;
    err.cause = e;
    throw err;
  }
}
