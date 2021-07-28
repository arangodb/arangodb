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

var MochaContext = require('@arangodb/mocha/context');
var MochaSuite = require('@arangodb/mocha/suite');
var MochaRunner = require('@arangodb/mocha/runner');
var createStatsCollector = require('@arangodb/mocha/stats-collector');

exports.reporters = {
  stream: require('@arangodb/mocha/reporter/stream'),
  suite: require('@arangodb/mocha/reporter/suite'),
  xunit: require('@arangodb/mocha/reporter/xunit'),
  tap: require('@arangodb/mocha/reporter/tap'),
  default: require('@arangodb/mocha/reporter/json')
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

  require('@arangodb/mocha/interface/bdd')(suite);
  require('@arangodb/mocha/interface/tdd')(suite);
  require('@arangodb/mocha/interface/exports')(suite);

  var options = {};
  var mocha = {
    options: options,
    grep (re) {
      if (re) {
        if (!Array.isArray(re)) {
          re = [re];
        }
        re = new RegExp(re.map(
          v => v.replace(/[|\\{}()[\]^$+*?.]/g, "\\$&")
        ).join("|"));
      }
      this.options.grep = re;
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

  var Reporter = exports.reporters[reporterName || "default"];
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
    for (const name of Object.getOwnPropertyNames(global)) {
      if (!globals.includes(name)) {
        delete global[name];
      }
    }
    for (const name of Object.getOwnPropertyNames(Object.prototype)) {
      if (!objectProps.includes(name)) {
        delete Object.prototype[name];
      }
    }
  }

  return runner.testResults || reporter.stats;
};
