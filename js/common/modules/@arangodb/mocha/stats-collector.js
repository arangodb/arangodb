'use strict';
// Based on mocha v6.1.3 under the MIT license.
// Original copyright (c) 2011-2018 JS Foundation and contributors,
// https://js.foundation

var Date = global.Date;

function createStatsCollector(runner) {
  var stats = {
    suites: 0,
    tests: 0,
    passes: 0,
    pending: 0,
    failures: 0
  };

  if (!runner) {
    throw new TypeError('Missing runner argument');
  }

  runner.stats = stats;

  runner.once('start', function() {
    stats.start = new Date();
  });
  runner.on('suite', function(suite) {
    suite.root || stats.suites++;
  });
  runner.on('pass', function() {
    stats.passes++;
  });
  runner.on('fail', function() {
    stats.failures++;
  });
  runner.on('pending', function() {
    stats.pending++;
  });
  runner.on('test end', function() {
    stats.tests++;
  });
  runner.once('end', function() {
    stats.end = new Date();
    stats.duration = stats.end - stats.start;
  });
}

module.exports = createStatsCollector;
