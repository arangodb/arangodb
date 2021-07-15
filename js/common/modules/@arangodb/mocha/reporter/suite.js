'use strict';
// Based on mocha v6.1.3 under the MIT license.
// Original copyright (c) 2011-2018 JS Foundation and contributors,
// https://js.foundation

const Reporter = require('@arangodb/mocha/reporter');
const { clean } = require('@arangodb/mocha/utils');

class SuiteReporter extends Reporter  {
  constructor(runner) {
    super(runner);
    var self = this;
    var suites = [];
    var currentSuite;
    runner.on('suite', function (suite) {
      var s = {
        title: suite.title,
        tests: [],
        suites: []
      };
      suites.unshift(s);
      if (currentSuite) {
        currentSuite.suites.push(s);
      }
      currentSuite = s;
    });
    runner.on('suite end', function () {
      var last = suites.shift();
      currentSuite = suites[0] || last;
    });
    runner.on('pending', function (test) {
      var t = clean(test);
      delete t.fullTitle;
      t.result = 'pending';
      currentSuite.tests.push(t);
    });
    runner.on('pass', function (test) {
      var t = clean(test);
      delete t.fullTitle;
      t.result = 'pass';
      currentSuite.tests.push(t);
    });
    runner.on('fail', function (test) {
      var t = clean(test);
      delete t.fullTitle;
      t.result = 'fail';
      currentSuite.tests.push(t);
    });
    runner.on('end', function () {
      runner.testResults = {
        stats: self.stats,
        suites: currentSuite ? currentSuite.suites : [],
        tests: currentSuite ? currentSuite.tests : []
      };
    });
  }
}

module.exports = SuiteReporter;