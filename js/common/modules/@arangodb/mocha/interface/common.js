'use strict';
// Based on mocha v6.1.3 under the MIT license.
// Original copyright (c) 2011-2018 JS Foundation and contributors,
// https://js.foundation

const Suite = require('@arangodb/mocha/suite');

function createMissingArgumentError(message, argument, expected) {
  var err = new TypeError(message);
  err.code = 'ERR_MOCHA_INVALID_ARG_TYPE';
  err.argument = argument;
  err.expected = expected;
  err.actual = typeof argument;
  return err;
}

module.exports = function(suites, context, mocha) {
  function shouldBeTested(suite) {
    return (
      !mocha.options.grep ||
      (mocha.options.grep &&
        mocha.options.grep.test(suite.fullTitle()) &&
        !mocha.options.invert)
    );
  }

  return {
    runWithSuite: function runWithSuite(suite) {
      return function run() {
        suite.run();
      };
    },

    before: function(name, fn) {
      suites[0].beforeAll(name, fn);
    },

    after: function(name, fn) {
      suites[0].afterAll(name, fn);
    },

    beforeEach: function(name, fn) {
      suites[0].beforeEach(name, fn);
    },

    afterEach: function(name, fn) {
      suites[0].afterEach(name, fn);
    },

    suite: {
      only: function only(opts) {
        opts.isOnly = true;
        return this.create(opts);
      },

      skip: function skip(opts) {
        opts.pending = true;
        return this.create(opts);
      },

      create: function create(opts) {
        var suite = Suite.create(suites[0], opts.title);
        suite.pending = Boolean(opts.pending);
        suite.file = opts.file;
        suites.unshift(suite);
        if (opts.isOnly) {
          if (mocha.options.forbidOnly && shouldBeTested(suite)) {
            throw new Error('`.only` forbidden');
          }

          suite.parent.appendOnlySuite(suite);
        }
        if (suite.pending) {
          if (mocha.options.forbidPending && shouldBeTested(suite)) {
            throw new Error('Pending test forbidden');
          }
        }
        if (typeof opts.fn === 'function') {
          opts.fn.call(suite);
          suites.shift();
        } else if (typeof opts.fn === 'undefined' && !suite.pending) {
          throw createMissingArgumentError(
            'Suite "' +
              suite.fullTitle() +
              '" was defined but no callback was supplied. ' +
              'Supply a callback or explicitly skip the suite.',
            'callback',
            'function'
          );
        } else if (!opts.fn && suite.pending) {
          suites.shift();
        }

        return suite;
      }
    },

    test: {
      only: function(mocha, test) {
        test.parent.appendOnlyTest(test);
        return test;
      },

      skip: function(title) {
        context.test(title);
      },

      retries: function(n) {
        context.retries(n);
      }
    }
  };
};
