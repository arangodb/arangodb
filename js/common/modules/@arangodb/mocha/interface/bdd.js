'use strict';
// Based on mocha v6.1.3 under the MIT license.
// Original copyright (c) 2011-2018 JS Foundation and contributors,
// https://js.foundation

const Test = require('@arangodb/mocha/test');

module.exports = function bddInterface(suite) {
  var suites = [suite];

  suite.on('pre-require', function(context, file, mocha) {
    var common = require('@arangodb/mocha/interface/common')(suites, context, mocha);

    context.before = common.before;
    context.after = common.after;
    context.beforeEach = common.beforeEach;
    context.afterEach = common.afterEach;
    context.run = mocha.options.delay && common.runWithSuite(suite);

    context.describe = context.context = function(title, fn) {
      return common.suite.create({
        title: title,
        file: file,
        fn: fn
      });
    };

    context.xdescribe = context.xcontext = context.describe.skip = function(
      title,
      fn
    ) {
      return common.suite.skip({
        title: title,
        file: file,
        fn: fn
      });
    };

    context.describe.only = function(title, fn) {
      return common.suite.only({
        title: title,
        file: file,
        fn: fn
      });
    };

    context.it = context.specify = function(title, fn) {
      var suite = suites[0];
      if (suite.isPending()) {
        fn = null;
      }
      var test = new Test(title, fn);
      test.file = file;
      suite.addTest(test);
      return test;
    };

    context.it.only = function(title, fn) {
      return common.test.only(mocha, context.it(title, fn));
    };

    context.xit = context.xspecify = context.it.skip = function(title) {
      return context.it(title);
    };

    context.it.retries = function(n) {
      context.retries(n);
    };
  });
};
