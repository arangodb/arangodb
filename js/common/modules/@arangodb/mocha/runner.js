'use strict';
// Based on mocha v6.1.3 under the MIT license.
// Original copyright (c) 2011-2018 JS Foundation and contributors,
// https://js.foundation

const util = require('util');
const EventEmitter = require('events').EventEmitter;
const Pending = require('@arangodb/mocha/pending');
const Runnable = require('@arangodb/mocha/runnable');

function createInvalidExceptionError(message, value) {
  var err = new Error(message);
  err.code = 'ERR_MOCHA_INVALID_EXCEPTION';
  err.valueType = typeof value;
  err.value = value;
  return err;
}

var globals = [
  'setTimeout',
  'clearTimeout',
  'setInterval',
  'clearInterval',
  'XMLHttpRequest',
  'Date',
  'setImmediate',
  'clearImmediate'
];

class Runner extends EventEmitter {
  constructor(suite, delay) {
    super();
    var self = this;
    this._globals = [];
    this._abort = false;
    this._delay = delay;
    this.suite = suite;
    this.started = false;
    this.total = suite.total();
    this.failures = 0;
    this.on('test end', function(test) {
      self.checkGlobals(test);
    });
    this.on('hook end', function(hook) {
      self.checkGlobals(hook);
    });
    this._defaultGrep = /.*/;
    this.grep(this._defaultGrep);
    this.globals(this.globalProps().concat(extraGlobals()));
  }

  grep(re, invert) {
    this._grep = re;
    this._invert = invert;
    this.total = this.grepTotal(this.suite);
    return this;
  }

  grepTotal(suite) {
    var self = this;
    var total = 0;

    suite.eachTest(function(test) {
      var match = self._grep.test(test.fullTitle());
      if (self._invert) {
        match = !match;
      }
      if (match) {
        total++;
      }
    });

    return total;
  }

  globalProps() {
    var props = Object.keys(global);

    for (var i = 0; i < globals.length; ++i) {
      if (~props.indexOf(globals[i])) {
        continue;
      }
      props.push(globals[i]);
    }

    return props;
  }

  globals(arr) {
    if (!arguments.length) {
      return this._globals;
    }
    this._globals = this._globals.concat(arr);
    return this;
  }

  checkGlobals(test) {
    if (this.ignoreLeaks) {
      return;
    }
    var ok = this._globals;

    var globals = this.globalProps();
    var leaks;

    if (test) {
      ok = ok.concat(test._allowedGlobals || []);
    }

    if (this.prevGlobalsLength === globals.length) {
      return;
    }
    this.prevGlobalsLength = globals.length;

    leaks = filterLeaks(ok, globals);
    this._globals = this._globals.concat(leaks);

    if (leaks.length) {
      var format = (
        leaks.length === 1
        ? 'global leak detected: %s'
        : 'global leaks detected: %s'
      );
      var error = new Error(util.format(format, leaks.map(leak => `'${leak}'`).join(', ')));
      this.fail(test, error);
    }
  }

  fail(test, err) {
    if (test.isPending()) {
      return;
    }

    ++this.failures;
    test.state = 'failed';

    if (!isError(err)) {
      err = thrown2Error(err);
    }

    // TODO filter stack trace

    this.emit('fail', test, err);
  }

  failHook(hook, err) {
    hook.originalTitle = hook.originalTitle || hook.title;
    if (hook.ctx && hook.ctx.currentTest) {
      hook.title =
        hook.originalTitle + ' for ' + `"${hook.ctx.currentTest.title}"`;
    } else {
      var parentTitle;
      if (hook.parent.title) {
        parentTitle = hook.parent.title;
      } else {
        parentTitle = hook.parent.root ? '{root}' : '';
      }
      hook.title = hook.originalTitle + ' in ' + `"parentTitle"`;
    }

    this.fail(hook, err);
  }

  hook(name, fn) {
    var suite = this.suite;
    var hooks = suite.getHooks(name);
    var self = this;

    function next(i) {
      var hook = hooks[i];
      if (!hook) {
        return fn();
      }
      self.currentRunnable = hook;

      if (name === 'beforeAll') {
        hook.ctx.currentTest = hook.parent.tests[0];
      } else if (name === 'afterAll') {
        hook.ctx.currentTest = hook.parent.tests[hook.parent.tests.length - 1];
      } else {
        hook.ctx.currentTest = self.test;
      }

      hook.allowUncaught = self.allowUncaught;

      self.emit('hook', hook);

      if (!hook.listeners('error').length) {
        hook.on('error', function(err) {
          self.failHook(hook, err);
        });
      }

      hook.run(function(err) {
        var testError = hook.error();
        if (testError) {
          self.fail(self.test, testError);
        }
        if (err) {
          if (err instanceof Pending) {
            if (name === 'afterAll') {
              throw new Error('Skipping a test within an "after all" hook is forbidden.');
            }
            if (name === 'beforeEach' || name === 'afterEach') {
              if (self.test) {
                self.test.pending = true;
              }
            } else {
              suite.tests.forEach(function(test) {
                test.pending = true;
              });
              suite.suites.forEach(function(suite) {
                suite.pending = true;
              });
              hook.pending = true;
            }
          } else {
            self.failHook(hook, err);
            return fn(err);
          }
        }
        self.emit('hook end', hook);
        delete hook.ctx.currentTest;
        next(++i);
      });
    }

    next(0);
  }

  hooks(name, suites, fn) {
    var self = this;
    var orig = this.suite;

    function next(suite) {
      self.suite = suite;

      if (!suite) {
        self.suite = orig;
        return fn();
      }

      self.hook(name, function(err) {
        if (err) {
          var errSuite = self.suite;
          self.suite = orig;
          return fn(err, errSuite);
        }

        next(suites.pop());
      });
    }

    next(suites.pop());
  }

  hookUp(name, fn) {
    var suites = [this.suite].concat(this.parents()).reverse();
    this.hooks(name, suites, fn);
  }

  hookDown(name, fn) {
    var suites = [this.suite].concat(this.parents());
    this.hooks(name, suites, fn);
  }

  parents() {
    var suite = this.suite;
    var suites = [];
    while (suite.parent) {
      suite = suite.parent;
      suites.push(suite);
    }
    return suites;
  }

  runTest(fn) {
    var self = this;
    var test = this.test;

    if (!test) {
      return;
    }

    var suite = this.parents().reverse()[0] || this.suite;
    if (this.forbidOnly && suite.hasOnly()) {
      fn(new Error('`.only` forbidden'));
      return;
    }
    test.on('error', function(err) {
      self.fail(test, err);
    });
    if (this.allowUncaught) {
      test.allowUncaught = true;
      return test.run(fn);
    }
    try {
      test.run(fn);
    } catch (err) {
      fn(err);
    }
  }

  runTests(suite, fn) {
    var self = this;
    var tests = suite.tests.slice();
    var test;

    function hookErr(_, errSuite, after) {
      var orig = self.suite;

      self.suite = after ? errSuite.parent : errSuite;

      if (self.suite) {
        self.hookUp('afterEach', function(err2, errSuite2) {
          self.suite = orig;
          if (err2) {
            return hookErr(err2, errSuite2, true);
          }
          fn(errSuite);
        });
      } else {
        self.suite = orig;
        fn(errSuite);
      }
    }

    function next(err, errSuite) {
      if (self.failures && suite._bail) {
        tests = [];
      }

      if (self._abort) {
        return fn();
      }

      if (err) {
        return hookErr(err, errSuite, true);
      }

      test = tests.shift();

      if (!test) {
        return fn();
      }

      var match = self._grep.test(test.fullTitle());
      if (self._invert) {
        match = !match;
      }
      if (!match) {
        next();
        return;
      }

      if (test.isPending()) {
        if (self.forbidPending) {
          test.isPending = () => false;
          self.fail(test, new Error('Pending test forbidden'));
          delete test.isPending;
        } else {
          self.emit('pending', test);
        }
        self.emit('test end', test);
        return next();
      }

      self.emit('test', (self.test = test));
      self.hookDown('beforeEach', function(err, errSuite) {
        if (test.isPending()) {
          if (self.forbidPending) {
            test.isPending = () => false;
            self.fail(test, new Error('Pending test forbidden'));
            delete test.isPending;
          } else {
            self.emit('pending', test);
          }
          self.emit('test end', test);
          return next();
        }
        if (err) {
          return hookErr(err, errSuite, false);
        }
        self.currentRunnable = self.test;
        self.runTest(function(err) {
          test = self.test;
          if (err) {
            var retry = test.currentRetry();
            if (err instanceof Pending && self.forbidPending) {
              self.fail(test, new Error('Pending test forbidden'));
            } else if (err instanceof Pending) {
              test.pending = true;
              self.emit('pending', test);
            } else if (retry < test.retries()) {
              var clonedTest = test.clone();
              clonedTest.currentRetry(retry + 1);
              tests.unshift(clonedTest);

              self.emit('retry', test, err);

              return self.hookUp('afterEach', next);
            } else {
              self.fail(test, err);
            }
            self.emit('test end', test);

            if (err instanceof Pending) {
              return next();
            }

            return self.hookUp('afterEach', next);
          }

          test.state = 'passed';
          self.emit('pass', test);
          self.emit('test end', test);
          self.hookUp('afterEach', next);
        });
      });
    }

    this.next = next;
    this.hookErr = hookErr;
    next();
  }

  runSuite(suite, fn) {
    var i = 0;
    var self = this;
    var total = this.grepTotal(suite);
    var afterAllHookCalled = false;


    if (!total || (self.failures && suite._bail)) {
      return fn();
    }

    this.emit('suite', (this.suite = suite));

    function next(errSuite) {
      if (errSuite) {
        if (errSuite === suite) {
          return done();
        }
        return done(errSuite);
      }

      if (self._abort) {
        return done();
      }

      var curr = suite.suites[i++];
      if (!curr) {
        return done();
      }

      self.runSuite(curr, next);
    }

    function done(errSuite) {
      self.suite = suite;
      self.nextSuite = next;

      if (afterAllHookCalled) {
        fn(errSuite);
      } else {
        afterAllHookCalled = true;

        delete self.test;

        self.hook('afterAll', function() {
          self.emit('suite end', suite);
          fn(errSuite);
        });
      }
    }

    this.nextSuite = next;

    this.hook('beforeAll', function(err) {
      if (err) {
        return done();
      }
      self.runTests(suite, next);
    });
  }

  uncaught(err) {
    if (err instanceof Pending) {
      return;
    }
    if (!err) {
      err = createInvalidExceptionError(
        'Caught falsy/undefined exception which would otherwise be uncaught. No stack trace found; try a debugger',
        err
      );
    }

    if (!isError(err)) {
      err = thrown2Error(err);
    }
    err.uncaught = true;

    var runnable = this.currentRunnable;

    if (!runnable) {
      runnable = new Runnable('Uncaught error outside test suite');
      runnable.parent = this.suite;

      if (this.started) {
        this.fail(runnable, err);
      } else {
        this.emit('start');
        this.fail(runnable, err);
        this.emit('end');
      }

      return;
    }

    if (runnable.isFailed() || runnable.isPending()) {
      return;
    }
    var alreadyPassed = runnable.isPassed();
    this.fail(runnable, err);
    if (!alreadyPassed) {
      if (runnable.type === 'test') {
        this.emit('test end', runnable);
        this.hookUp('afterEach', this.next);
        return;
      }

      var errSuite = this.suite;

      if (runnable.fullTitle().indexOf('after each') > -1) {
        return this.hookErr(err, errSuite, true);
      }
      if (runnable.fullTitle().indexOf('before each') > -1) {
        return this.hookErr(err, errSuite, false);
      }
      return this.nextSuite(errSuite);
    }

    this.emit('end');
  }

  run(fn) {
    var self = this;
    var rootSuite = this.suite;

    fn = fn || function() {};

    function uncaught(err) {
      self.uncaught(err);
    }

    function start() {
      if (rootSuite.hasOnly()) {
        rootSuite.filterOnly();
      }
      self.started = true;
      if (self._delay) {
        self.emit('ready');
      }
      self.emit('start');

      self.runSuite(rootSuite, function() {
        self.emit('end');
      });
    }


    this.on('suite end', function(suite) {
      suite.cleanReferences();
    });

    this.on('end', function() {
      process.removeListener('uncaughtException', uncaught);
      fn(self.failures);
    });

    process.on('uncaughtException', uncaught);

    if (this._delay) {
      this.emit('waiting', rootSuite);
      rootSuite.once('run', start);
    } else {
      start();
    }

    return this;
  }

  abort() {
    this._abort = true;

    return this;
  }
}

function filterLeaks(ok, globals) {
  return globals.filter(function(key) {
    if (/^\d+/.test(key)) {
      return false;
    }

    if (/^mocha-/.test(key)) {
      return false;
    }

    var matched = ok.filter(function(ok) {
      if (~ok.indexOf('*')) {
        return key.indexOf(ok.split('*')[0]) === 0;
      }
      return key === ok;
    });
    return !matched.length && (!global.navigator || key !== 'onerror');
  });
}

function isError(err) {
  return err instanceof Error || (err && typeof err.message === 'string');
}

function thrown2Error(err) {
  return new Error(
    util.inspect(err) + ' was thrown, throw an Error :)'
  );
}

function extraGlobals() {
  if (typeof process === 'object' && typeof process.version === 'string') {
    var parts = process.version.split('.');
    var nodeVersion = parts.reduce(function(a, v) {
      return (a << 8) | v;
    });

    // 'errno' was renamed to process._errno in v0.9.11.
    if (nodeVersion < 0x00090b) {
      return ['errno'];
    }
  }

  return [];
}

module.exports = Runner;