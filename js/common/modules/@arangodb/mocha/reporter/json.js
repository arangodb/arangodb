'use strict';
// Based on mocha v6.1.3 under the MIT license.
// Original copyright (c) 2011-2018 JS Foundation and contributors,
// https://js.foundation

const Reporter = require('@arangodb/mocha/reporter');
const { clean } = require('@arangodb/mocha/utils');

class JsonReporter extends Reporter {
  constructor(runner, options) {
    super(runner, options);
    var self = this;
    var tests = [];
    var pending = [];
    var failures = [];
    var passes = [];

    runner.on('test end', function(test) {
      tests.push(test);
    });

    runner.on('pass', function(test) {
      passes.push(test);
    });

    runner.on('fail', function(test) {
      failures.push(test);
    });

    runner.on('pending', function(test) {
      pending.push(test);
    });

    runner.once('end', function() {
      var obj = {
        stats: self.stats,
        tests: tests.map(clean),
        pending: pending.map(clean),
        failures: failures.map(clean),
        passes: passes.map(clean)
      };

      runner.testResults = obj;
    });
  }
}

module.exports = JsonReporter;