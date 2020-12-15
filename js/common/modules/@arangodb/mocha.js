'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief ArangoDB Mocha integration
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2015-2016 triAGENS GmbH, Cologne, Germany
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
// / @author Copyright 2015-2016, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var interfaces = require('mocha/lib/interfaces');
var MochaContext = require('mocha/lib/context');
var MochaSuite = require('mocha/lib/suite');
var MochaRunner = require('mocha/lib/runner');
var BaseReporter = require('mocha/lib/reporters/base');
var DefaultReporter = require('mocha/lib/reporters/json');
var createStatsCollector = require('mocha/lib/stats-collector');
var escapeRe = require('mocha/node_modules/escape-string-regexp');

function notIn (arr) {
  return function (item) {
    return arr.indexOf(item) === -1;
  };
}

function deleteFrom (obj) {
  return function (key) {
    delete obj[key];
  };
}

exports.reporters = {
  stream: StreamReporter,
  suite: SuiteReporter,
  xunit: XunitReporter,
  tap: TapReporter,
  default: DefaultReporter
};

exports.run = function runMochaTests (run, files, reporterName, grep) {
  if (!Array.isArray(files)) {
    files = [files];
  }

  if (reporterName && !exports.reporters[reporterName]) {
    throw new Error(`Unknown test reporter: "${
      reporterName
    }". Known reporters: ${
      Object.keys(exports.reporters).join(', ')
    }`);
  }

  var suite = new MochaSuite('', new MochaContext());
  suite.timeout(0);
  suite.bail(false);

  interfaces.bdd(suite);
  interfaces.tdd(suite);
  interfaces.exports(suite);

  var options = {};
  var mocha = {
    options: options,
    grep (re) {
      this.options.grep = (
        typeof re === 'string'
        ? new RegExp(escapeRe(re))
        : re
      );
      return this;
    },
    invert () {
      this.options.invert = true;
      return this;
    }
  };
  if (grep) {
    mocha.grep(grep);
  }

  // Clean up after chai.should(), etc
  var globals = Object.getOwnPropertyNames(global);
  var objectProps = Object.getOwnPropertyNames(Object.prototype);
  // Monkeypatch process.stdout.write for mocha's JSON reporter
  var _stdoutWrite = global.process.stdout.write;
  global.process.stdout.write = function () {};

  var Reporter = reporterName ? exports.reporters[reporterName] : exports.reporters.default;
  var reporter, runner;

  try {
    files.forEach(function (file) {
      var context = {};
      suite.emit('pre-require', context, file, mocha);
      suite.emit('require', run(file, context), file, mocha);
      suite.emit('post-require', global, file, mocha);
    });

    runner = new MochaRunner(suite, false);
    createStatsCollector(runner);
    runner.ignoreLeaks = true;
    reporter = new Reporter(runner, options);
    if (options.grep) {
      runner.grep(options.grep, options.invert);
    }
    runner.run();
  } finally {
    Object.getOwnPropertyNames(global)
      .filter(notIn(globals))
      .forEach(deleteFrom(global));
    Object.getOwnPropertyNames(Object.prototype)
      .filter(notIn(objectProps))
      .forEach(deleteFrom(Object.prototype));
    global.process.stdout.write = _stdoutWrite;
  }

  return runner.testResults || reporter.stats;
};

function StreamReporter (runner) {
  var self = this;
  BaseReporter.call(this, runner);
  var items = [];
  var total = runner.total;
  runner.on('start', function () {
    items.push(['start', {total: total}]);
  });
  runner.on('pass', function (test) {
    var t = clean(test);
    delete t.err;
    items.push(['pass', t]);
  });
  runner.on('fail', function (test, err) {
    var t = clean(test);
    t.err = err.message;
    items.push(['fail', t]);
  });
  runner.on('end', function () {
    items.push(['end', self.stats]);
  });
  runner.testResults = items;
}

function SuiteReporter (runner) {
  var self = this;
  BaseReporter.call(this, runner);
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

function XunitReporter (runner) {
  DefaultReporter.call(this, runner);
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

function TapReporter (runner) {
  DefaultReporter.call(this, runner);
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

// via https://github.com/mochajs/mocha/blob/c6747a/lib/reporters/json.js
// Copyright (c) 2011-2015 TJ Holowaychuk <tj@vision-media.ca>
// The MIT License

/* *
 * Return a plain-object representation of `test`
 * free of cyclic properties etc.
 *
 * @param {Object} test
 * @return {Object}
 * @api private
 */

function clean (test) {
  return {
    title: test.title,
    fullTitle: test.fullTitle(),
    duration: test.duration,
    err: errorJSON(test.err || {})
  };
}

/* *
 * Transform `error` into a JSON object.
 * @param {Error} err
 * @return {Object}
 */

function errorJSON (err) {
  var res = {};
  Object.getOwnPropertyNames(err).forEach(function (key) {
    res[key] = err[key];
  }, err);
  return res;
}
