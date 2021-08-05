'use strict';
// Based on mocha v6.1.3 under the MIT license.
// Original copyright (c) 2011-2018 JS Foundation and contributors,
// https://js.foundation

const Reporter = require('@arangodb/mocha/reporter');
const { clean } = require('@arangodb/mocha/utils');

class StreamReporter extends Reporter {
  constructor (runner) {
    super(runner);
    var self = this;
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
}

module.exports = StreamReporter;