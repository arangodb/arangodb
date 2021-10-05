'use strict';
// Based on mocha v6.1.3 under the MIT license.
// Original copyright (c) 2011-2018 JS Foundation and contributors,
// https://js.foundation

/**
 * Module dependencies.
 */
const EventEmitter = require('events').EventEmitter;
const milliseconds = require('ms');
const Hook = require('@arangodb/mocha/hook');

function createInvalidArgumentTypeError(message, argument, expected) {
  var err = new TypeError(message);
  err.code = 'ERR_MOCHA_INVALID_ARG_TYPE';
  err.argument = argument;
  err.expected = expected;
  err.actual = typeof argument;
  return err;
}

class Suite extends EventEmitter {
  constructor(title, parentContext, isRoot) {
    super();
    if (typeof title !== "string") {
      throw createInvalidArgumentTypeError(
        'Suite argument "title" must be a string. Received type "' +
          typeof title +
          '"',
        'title',
        'string'
      );
    }
    this.title = title;
    this.ctx = Object.create(parentContext || null);
    this.suites = [];
    this.tests = [];
    this.pending = false;
    this._beforeEach = [];
    this._beforeAll = [];
    this._afterEach = [];
    this._afterAll = [];
    this.root = isRoot === true;
    this._timeout = 2000;
    this._enableTimeouts = true;
    this._slow = 75;
    this._bail = false;
    this._retries = -1;
    this._onlyTests = [];
    this._onlySuites = [];
    this.delayed = false;
  }

  clone() {
    var suite = new Suite(this.title);
    suite.ctx = this.ctx;
    suite.root = this.root;
    suite.timeout(this.timeout());
    suite.retries(this.retries());
    suite.enableTimeouts(this.enableTimeouts());
    suite.slow(this.slow());
    suite.bail(this.bail());
    return suite;
  }

  timeout(ms) {
    if (!arguments.length) {
      return this._timeout;
    }
    if (ms.toString() === '0') {
      this._enableTimeouts = false;
    }
    if (typeof ms === 'string') {
      ms = milliseconds(ms);
    }
    this._timeout = parseInt(ms, 10);
    return this;
  }

  retries(n) {
    if (!arguments.length) {
      return this._retries;
    }
    this._retries = parseInt(n, 10) || 0;
    return this;
  }

  enableTimeouts(enabled) {
    if (!arguments.length) {
      return this._enableTimeouts;
    }
    this._enableTimeouts = enabled;
    return this;
  }

  slow(ms) {
    if (!arguments.length) {
      return this._slow;
    }
    if (typeof ms === 'string') {
      ms = milliseconds(ms);
    }
    this._slow = ms;
    return this;
  }

  bail(bail) {
    if (!arguments.length) {
      return this._bail;
    }
    this._bail = bail;
    return this;
  }

  isPending() {
    return this.pending || (this.parent && this.parent.isPending());
  }

  _createHook(title, fn) {
    var hook = new Hook(title, fn);
    hook.parent = this;
    hook.timeout(this.timeout());
    hook.retries(this.retries());
    hook.enableTimeouts(this.enableTimeouts());
    hook.slow(this.slow());
    hook.ctx = this.ctx;
    hook.file = this.file;
    return hook;
  }

  beforeAll(title, fn) {
    if (this.isPending()) {
      return this;
    }
    if (typeof title === 'function') {
      fn = title;
      title = fn.name;
    }
    title = '"before all" hook' + (title ? ': ' + title : '');

    var hook = this._createHook(title, fn);
    this._beforeAll.push(hook);
    this.emit('beforeAll', hook);
    return this;
  }

  afterAll(title, fn) {
    if (this.isPending()) {
      return this;
    }
    if (typeof title === 'function') {
      fn = title;
      title = fn.name;
    }
    title = '"after all" hook' + (title ? ': ' + title : '');

    var hook = this._createHook(title, fn);
    this._afterAll.push(hook);
    this.emit('afterAll', hook);
    return this;
  }

  beforeEach(title, fn) {
    if (this.isPending()) {
      return this;
    }
    if (typeof title === 'function') {
      fn = title;
      title = fn.name;
    }
    title = '"before each" hook' + (title ? ': ' + title : '');

    var hook = this._createHook(title, fn);
    this._beforeEach.push(hook);
    this.emit('beforeEach', hook);
    return this;
  }

  afterEach(title, fn) {
    if (this.isPending()) {
      return this;
    }
    if (typeof title === 'function') {
      fn = title;
      title = fn.name;
    }
    title = '"after each" hook' + (title ? ': ' + title : '');

    var hook = this._createHook(title, fn);
    this._afterEach.push(hook);
    this.emit('afterEach', hook);
    return this;
  }

  addSuite(suite) {
    suite.parent = this;
    suite.root = false;
    suite.timeout(this.timeout());
    suite.retries(this.retries());
    suite.enableTimeouts(this.enableTimeouts());
    suite.slow(this.slow());
    suite.bail(this.bail());
    this.suites.push(suite);
    this.emit('suite', suite);
    return this;
  }

  addTest(test) {
    test.parent = this;
    test.timeout(this.timeout());
    test.retries(this.retries());
    test.enableTimeouts(this.enableTimeouts());
    test.slow(this.slow());
    test.ctx = this.ctx;
    this.tests.push(test);
    this.emit('test', test);
    return this;
  }

  fullTitle() {
    return this.titlePath().join(' ');
  }

  titlePath() {
    var result = [];
    if (this.parent) {
      result = result.concat(this.parent.titlePath());
    }
    if (!this.root) {
      result.push(this.title);
    }
    return result;
  }

  total() {
    return (
      this.suites.reduce(function(sum, suite) {
        return sum + suite.total();
      }, 0) + this.tests.length
    );
  }

  eachTest(fn) {
    this.tests.forEach(fn);
    this.suites.forEach(function(suite) {
      suite.eachTest(fn);
    });
    return this;
  }

  run() {
    if (this.root) {
      this.emit('run');
    }
  }

  hasOnly() {
    return (
      this._onlyTests.length > 0 ||
      this._onlySuites.length > 0 ||
      this.suites.some(function(suite) {
        return suite.hasOnly();
      })
    );
  }

  filterOnly() {
    if (this._onlyTests.length) {
      this.tests = this._onlyTests;
      this.suites = [];
    } else {
      this.tests = [];
      this._onlySuites.forEach(function(onlySuite) {
        if (onlySuite.hasOnly()) {
          onlySuite.filterOnly();
        }
      });
      var onlySuites = this._onlySuites;
      this.suites = this.suites.filter(function(childSuite) {
        return onlySuites.indexOf(childSuite) !== -1 || childSuite.filterOnly();
      });
    }
    return this.tests.length > 0 || this.suites.length > 0;
  }

  appendOnlySuite(suite) {
    this._onlySuites.push(suite);
  }

  appendOnlyTest(test) {
    this._onlyTests.push(test);
  }

  getHooks(name) {
    return this['_' + name];
  }

  cleanReferences() {
    function cleanArrReferences(arr) {
      for (var i = 0; i < arr.length; i++) {
        delete arr[i].fn;
      }
    }

    if (Array.isArray(this._beforeAll)) {
      cleanArrReferences(this._beforeAll);
    }

    if (Array.isArray(this._beforeEach)) {
      cleanArrReferences(this._beforeEach);
    }

    if (Array.isArray(this._afterAll)) {
      cleanArrReferences(this._afterAll);
    }

    if (Array.isArray(this._afterEach)) {
      cleanArrReferences(this._afterEach);
    }

    for (var i = 0; i < this.tests.length; i++) {
      delete this.tests[i].fn;
    }
  }
}

module.exports = Suite;

Suite.create = function(parent, title) {
  var suite = new Suite(title, parent.ctx);
  suite.parent = parent;
  title = suite.fullTitle();
  parent.addSuite(suite);
  return suite;
};