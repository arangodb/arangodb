'use strict';
// Based on mocha v6.1.3 under the MIT license.
// Original copyright (c) 2011-2018 JS Foundation and contributors,
// https://js.foundation

const util = require('util');

class BaseReporter {
  constructor(runner, options) {
    var failures = (this.failures = []);

    if (!runner) {
      throw new TypeError('Missing runner argument');
    }
    this.options = options || {};
    this.runner = runner;
    this.stats = runner.stats; // assigned so Reporters keep a closer reference

    runner.on("pass", function(test) {
      if (test.duration > test.slow()) {
        test.speed = 'slow';
      } else if (test.duration > test.slow() / 2) {
        test.speed = 'medium';
      } else {
        test.speed = 'fast';
      }
    });

    runner.on("fail", function(test, err) {
      if (
        err &&
        err.showDiff !== false &&
        String(err.actual) === String(err.expected) &&
        err.expected !== undefined &&
        (typeof err.actual !== "string" || typeof err.expected !== "string")
      ) {
        err.actual = util.inspect(err.actual);
        err.expected = util.inspect(err.expected);
      }
      test.err = err;
      failures.push(test);
    });
  }
}

module.exports = BaseReporter;
