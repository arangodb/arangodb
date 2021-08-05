'use strict';
// Based on mocha v6.1.3 under the MIT license.
// Original copyright (c) 2011-2018 JS Foundation and contributors,
// https://js.foundation

const JsonReporter = require('@arangodb/mocha/reporter/json');

class TapReporter extends JsonReporter {
  constructor(runner) {
    super(runner);
    runner.on('end', function () {
      const result = runner.testResults;
      const lines = [];
      lines.push(`1..${result.stats.tests}`);
      for (let i = 0; i < result.tests.length; i++) {
        const test = result.tests[i];
        if (test.err.stack) {
          lines.push(`not ok ${i + 1} ${test.fullTitle}`);
          const [message, ...stack] = test.err.stack.split('\n');
          lines.push('  ' + message);
          for (const line of stack) {
            lines.push('  ' + line);
          }
        } else if (test.duration === undefined) {
          lines.push(`ok ${i + 1} ${test.fullTitle} # SKIP -`);
        } else {
          lines.push(`ok ${i + 1} ${test.fullTitle}`);
        }
      }
      lines.push(`# tests ${result.stats.tests}`);
      lines.push(`# pass ${result.stats.passes}`);
      lines.push(`# fail ${result.stats.failures}`);
      runner.testResults = lines;
    });
  }
}

module.exports = TapReporter;