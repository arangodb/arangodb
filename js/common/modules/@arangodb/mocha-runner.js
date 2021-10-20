/* global print */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief ArangoDB Mocha integration
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2016 triAGENS GmbH, Cologne, Germany
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
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Alan Plum
// / @author Copyright 2016, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const Module = require('module');
const runTests = require('@arangodb/mocha').run;
const indent = require('@arangodb/util').indentation;
const COLORS = require('internal').COLORS;

const $_MODULE_CONTEXT = Symbol.for('@arangodb/module.context');

module.exports = function (files, returnJson, grep) {
  const results = runTests(run, files, 'suite', grep);
  print();
  logSuite(results);
  logStats(results.stats);
  print();
  if (returnJson) {
    return buildJson(results);
  }
  return results.stats.failures === 0;
};

function logStats (stats) {
  print(`${
    stats.failures
    ? `${COLORS.COLOR_RED}[FAIL]${COLORS.COLOR_RESET}`
    : `${COLORS.COLOR_GREEN}[PASS]${COLORS.COLOR_RESET}`
  } Completed ${
    COLORS.COLOR_BOLD_WHITE}${stats.tests}${COLORS.COLOR_RESET} tests in ${
    COLORS.COLOR_BOLD_WHITE}${stats.duration + 'ms'}${COLORS.COLOR_RESET} (${
    COLORS.COLOR_GREEN}${stats.passes}${COLORS.COLOR_RESET}|${
    COLORS.COLOR_RED}${stats.failures}${COLORS.COLOR_RESET}|${
    COLORS.COLOR_CYAN}${stats.pending}${COLORS.COLOR_RESET
  })`);
}

function logSuite (suite, indentLevel) {
  if (!indentLevel) {
    indentLevel = 0;
  }
  if (suite.title) {
    print(`${indent(indentLevel - 1)}${
      COLORS.COLOR_BOLD_WHITE}${suite.title}${COLORS.COLOR_RESET
    }`);
  }
  for (let test of suite.tests) {
    if (test.result === 'pass') {
      print(`${indent(indentLevel)}${
        COLORS.COLOR_GREEN}[PASS] ${test.title}${COLORS.COLOR_RESET
      }`);
    } else if (test.result === 'pending') {
      print(`${indent(indentLevel)}${
        COLORS.COLOR_CYAN}[SKIP] ${test.title}${COLORS.COLOR_RESET
      }`);
    } else {
      print(`${indent(indentLevel)}${
        COLORS.COLOR_RED}[${test.result.toUpperCase()}] ${test.title}${COLORS.COLOR_RESET
      }`);
      for (let line of test.err.stack.split(/\n/)) {
        print(`${indent(indentLevel + 1)}${
          COLORS.COLOR_RED}${line}${COLORS.COLOR_RESET
        }`);
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

function buildJson (results) {
  const result = {
    duration: results.stats.duration,
    status: !results.stats.failures,
    failed: results.stats.failures,
    total: results.stats.tests,
    totalSetUp: 0, // we can't get setup time spent in mocha
    totalTearDown: 0
  };
  for (const test of findTests(results)) {
    const name = test[0];
    const stats = test[1];
    let newName = name;
    let i = 1;
    while (result.hasOwnProperty(newName)) {
      newName = `${name} ${i++}`;
    }
    result[newName] = {
      status: stats.result === 'pass' || stats.result === 'pending',
      duration: stats.duration
    };
    if (stats.result !== 'pass' && stats.result !== 'pending') {
      result[newName].message = stats.err.stack;
    }
  }
  return result;
}

function findTests (suite, prefix) {
  prefix = prefix ? prefix + ' ' : '';
  const results = [];
  for (const test of suite.tests) {
    results.push([prefix + test.title, test]);
  }
  for (const sub of suite.suites) {
    for (const res of findTests(sub, prefix + sub.title)) {
      results.push(res);
    }
  }
  return results;
}

function run (filename, context) {
  const module = new Module(filename);

  if (context) {
    Object.keys(context).forEach(function (key) {
      module[$_MODULE_CONTEXT][key] = context[key];
    });
  }

  module.load(filename);
  return module.exports;
}
