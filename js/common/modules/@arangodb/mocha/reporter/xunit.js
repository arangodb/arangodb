'use strict';
// Based on mocha v6.1.3 under the MIT license.
// Original copyright (c) 2011-2018 JS Foundation and contributors,
// https://js.foundation

const JsonReporter = require('@arangodb/mocha/reporter/json');

class XunitReporter extends JsonReporter {
  constructor(runner) {
    super(runner);
    let start;
    runner.on('start', function () {
      start = new Date().toISOString();
    });
    runner.on('end', function () {
      const result = runner.testResults;
      const tests = [];
      for (const test of result.tests) {
        const parentName = test.fullTitle.slice(0, -(test.title.length + 1)) || 'global';
        const testcase = ['testcase', {
          classname: parentName,
          name: test.title,
          time: test.duration || 0
        }];
        if (test.err.stack) {
          const [cause, ...stack] = test.err.stack.split('\n');
          const [type, ...message] = cause.split(': ');
          testcase.push(['failure', {
            message: message.join(': '),
            type
          }, [cause, ...stack].join('\n')]);
        }
        tests.push(testcase);
      }
      runner.testResults = ['testsuite', {
        timestamp: start,
        tests: result.stats.tests || 0,
        errors: 0,
        failures: result.stats.failures || 0,
        skip: result.stats.pending || 0,
        time: result.stats.duration || 0
      }, ...tests];
    });
  }
}
module.exports = XunitReporter;