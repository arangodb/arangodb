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

var reporters = {
  stream: StreamReporter,
  suite: SuiteReporter,
  default: DefaultReporter
};

exports.run = function runMochaTests (run, files, reporterName) {
  if (!Array.isArray(files)) {
    files = [files];
  }

  if (reporterName && !reporters[reporterName]) {
    throw new Error(
      'Unknown test reporter: ' + reporterName
      + ' Known reporters: ' + Object.keys(reporters).join(', ')
    );
  }

  var suite = new MochaSuite('', new MochaContext());
  suite.timeout(0);
  suite.bail(false);

  Object.keys(interfaces).forEach(function (key) {
    interfaces[key](suite);
  });

  var options = {};
  var mocha = {
    options: options,
    grep(re) {
      this.options.grep = typeof re === 'string' ? new RegExp(escapeRe(re)) : re;
      return this;
    },
    invert() {
      this.options.invert = true;
      return this;
    }
  };

  // Clean up after chai.should(), etc
  var globals = Object.getOwnPropertyNames(global);
  var objectProps = Object.getOwnPropertyNames(Object.prototype);
  // Monkeypatch process.stdout.write for mocha's JSON reporter
  var _stdoutWrite = global.process.stdout.write;
  global.process.stdout.write = function () {};

  var Reporter = reporterName ? reporters[reporterName] : reporters.default;
  var reporter, runner;

  try {
    files.forEach(function (file) {
      var context = {};
      suite.emit('pre-require', context, file, mocha);
      suite.emit('require', run(file, context), file, mocha);
      suite.emit('post-require', global, file, mocha);
    });

    runner = new MochaRunner(suite, false);
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
