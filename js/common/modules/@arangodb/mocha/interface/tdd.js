'use strict';
// Based on mocha v6.1.3 under the MIT license.
// Original copyright (c) 2011-2018 JS Foundation and contributors,
// https://js.foundation

const Test = require('@arangodb/mocha/test');

module.exports = function(suite) {
  var suites = [suite];

  suite.on('pre-require', function(context, file, mocha) {
    var common = require('@arangodb/mocha/interface/common')(suites, context, mocha);

    context.setup = common.beforeEach;
    context.teardown = common.afterEach;
    context.suiteSetup = common.before;
    context.suiteTeardown = common.after;
    context.run = mocha.options.delay && common.runWithSuite(suite);

    context.suite = function(title, fn) {
      return common.suite.create({
        title: title,
        file: file,
        fn: fn
      });
    };

    context.suite.skip = function(title, fn) {
      return common.suite.skip({
        title: title,
        file: file,
        fn: fn
      });
    };

    context.suite.only = function(title, fn) {
      return common.suite.only({
        title: title,
        file: file,
        fn: fn
      });
    };

    context.test = function(title, fn) {
      var suite = suites[0];
      if (suite.isPending()) {
        fn = null;
      }
      var test = new Test(title, fn);
      test.file = file;
      suite.addTest(test);
      return test;
    };

    context.test.only = function(title, fn) {
      return common.test.only(mocha, context.test(title, fn));
    };

    context.test.skip = common.test.skip;
    context.test.retries = common.test.retries;
  });
};
