'use strict';
// Based on mocha v6.1.3 under the MIT license.
// Original copyright (c) 2011-2018 JS Foundation and contributors,
// https://js.foundation

const Runnable = require('@arangodb/mocha/runnable');

function createInvalidArgumentTypeError(message, argument, expected) {
  var err = new TypeError(message);
  err.code = 'ERR_MOCHA_INVALID_ARG_TYPE';
  err.argument = argument;
  err.expected = expected;
  err.actual = typeof argument;
  return err;
}

class Test extends Runnable {
  constructor(title, fn) {
    if (typeof title !== "string") {
    throw createInvalidArgumentTypeError(
      'Test argument "title" should be a string. Received type "' +
      typeof title +
      '"',
      'title',
      'string'
      );
    }
    super(title, fn);
    this.pending = !fn;
    this.type = 'test';
  }

  clone() {
    var test = new Test(this.title, this.fn);
    test.timeout(this.timeout());
    test.slow(this.slow());
    test.enableTimeouts(this.enableTimeouts());
    test.retries(this.retries());
    test.currentRetry(this.currentRetry());
    test.globals(this.globals());
    test.parent = this.parent;
    test.file = this.file;
    test.ctx = this.ctx;
    return test;
  }
}

module.exports = Test;