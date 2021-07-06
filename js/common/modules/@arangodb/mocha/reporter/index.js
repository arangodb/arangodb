'use strict';
const util = require('util');

/**
 * Constructs a new `Base` reporter instance.
 *
 * @description
 * All other reporters generally inherit from this reporter.
 *
 * @public
 * @class
 * @memberof Mocha.reporters
 * @param {Runner} runner - Instance triggers reporter actions.
 * @param {Object} [options] - runner options
 */
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
